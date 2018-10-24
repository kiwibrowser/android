// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/server_window.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ui/ws2/drag_drop_delegate.h"
#include "services/ui/ws2/embedding.h"
#include "services/ui/ws2/window_tree.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event_handler.h"
#include "ui/wm/core/capture_controller.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui::ws2::ServerWindow*);

namespace ui {
namespace ws2 {
namespace {
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ws2::ServerWindow,
                                   kServerWindowKey,
                                   nullptr);

// Returns true if |location| is in the non-client area (or outside the bounds
// of the window). A return value of false means the location is in the client
// area.
bool IsLocationInNonClientArea(const aura::Window* window,
                               const gfx::Point& location) {
  const ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  if (!server_window || !server_window->IsTopLevel())
    return false;

  // Locations outside the bounds, assume it's in extended hit test area, which
  // is non-client area.
  if (!gfx::Rect(window->bounds().size()).Contains(location))
    return true;

  gfx::Rect client_area(window->bounds().size());
  client_area.Inset(server_window->client_area());
  if (client_area.Contains(location))
    return false;

  for (const auto& rect : server_window->additional_client_areas()) {
    if (rect.Contains(location))
      return false;
  }
  return true;
}

bool IsPointerPressedEvent(const Event& event) {
  return event.type() == ET_MOUSE_PRESSED || event.type() == ET_TOUCH_PRESSED;
}

bool IsPointerEvent(const Event& event) {
  return event.IsMouseEvent() || event.IsTouchEvent();
}

bool IsLastMouseButtonRelease(const Event& event) {
  return event.type() == ET_MOUSE_RELEASED &&
         event.AsMouseEvent()->button_flags() ==
             event.AsMouseEvent()->changed_button_flags();
}

bool IsPointerReleased(const Event& event) {
  return IsLastMouseButtonRelease(event) || event.type() == ET_TOUCH_RELEASED;
}

PointerId GetPointerId(const Event& event) {
  if (event.IsMouseEvent())
    return MouseEvent::kMousePointerId;
  DCHECK(event.IsTouchEvent());
  return event.AsTouchEvent()->pointer_details().id;
}

// WindowTargeter used for ServerWindows. This is used for two purposes:
// . If the location is in the non-client area, then child Windows are not
//   considered. This is done to ensure the delegate of the window (which is
//   local) sees the event.
// . To ensure |WindowTree::intercepts_events_| is honored.
class ServerWindowTargeter : public aura::WindowTargeter {
 public:
  explicit ServerWindowTargeter(ServerWindow* server_window)
      : server_window_(server_window) {}
  ~ServerWindowTargeter() override = default;

  // aura::WindowTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* event_target,
                                      ui::Event* event) override {
    aura::Window* window = static_cast<aura::Window*>(event_target);
    DCHECK_EQ(window, server_window_->window());
    if (server_window_->DoesOwnerInterceptEvents()) {
      // If the owner intercepts events, then don't recurse (otherwise events
      // would go to a descendant).
      return event_target->CanAcceptEvent(*event) ? window : nullptr;
    }

    // Ensure events in the non-client area target the top-level window.
    // TopLevelEventHandler will ensure these are routed correctly.
    if (event->IsLocatedEvent() &&
        IsLocationInNonClientArea(window,
                                  event->AsLocatedEvent()->location())) {
      return window;
    }
    return aura::WindowTargeter::FindTargetForEvent(event_target, event);
  }

 private:
  ServerWindow* const server_window_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowTargeter);
};

// ServerWindowEventHandler is used to forward events to the client.
// ServerWindowEventHandler adds itself to the pre-phase to ensure it's
// considered before the Window's delegate (or other EventHandlers).
class ServerWindowEventHandler : public ui::EventHandler {
 public:
  explicit ServerWindowEventHandler(ServerWindow* server_window)
      : server_window_(server_window) {
    window()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
  }
  ~ServerWindowEventHandler() override {
    window()->RemovePreTargetHandler(this);
  }

  ServerWindow* server_window() { return server_window_; }
  aura::Window* window() { return server_window_->window(); }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // This code doesn't handle PointerEvents, because they should never be
    // generated at this layer.
    DCHECK(!event->IsPointerEvent());

    if (event->phase() != EP_PRETARGET) {
      // All work is done in the pre-phase. If this branch is hit, it means
      // event propagation was not stopped, and normal processing should
      // continue. Early out to avoid sending the event to the client again.
      return;
    }

    if (HandleInterceptedEvent(event) || ShouldIgnoreEvent(*event))
      return;

    auto* owning = server_window_->owning_window_tree();
    auto* embedded = server_window_->embedded_window_tree();
    WindowTree* target_client = nullptr;
    if (server_window_->DoesOwnerInterceptEvents()) {
      // A client that intercepts events, always gets the event regardless of
      // focus/capture.
      target_client = owning;
    } else if (event->IsKeyEvent()) {
      if (!server_window_->focus_owner())
        return;  // The local environment is going to process the event.
      target_client = server_window_->focus_owner();
    } else if (server_window()->capture_owner()) {
      target_client = server_window()->capture_owner();
    } else {
      // Prefer embedded over owner.
      target_client = !embedded ? owning : embedded;
    }
    DCHECK(target_client);
    target_client->SendEventToClient(window(), *event);

    // The event was forwarded to the remote client. We don't want it handled
    // locally too.
    event->StopPropagation();
  }

 protected:
  // Returns true if the event should be ignored (not forwarded to the client).
  bool ShouldIgnoreEvent(const ui::Event& event) {
    if (static_cast<aura::Window*>(event.target()) != window()) {
      // As ServerWindow is a EP_PRETARGET EventHandler it gets events *before*
      // descendants. Ignore all such events, and only process when
      // window() is the the target.
      return true;
    }
    return ShouldIgnoreEventType(event.type());
  }

  bool ShouldIgnoreEventType(EventType type) const {
    // WindowTreeClient takes care of sending ET_MOUSE_CAPTURE_CHANGED at the
    // right point. The enter events are effectively synthetic, and indirectly
    // generated in the client as the result of a move event.
    switch (type) {
      case ET_MOUSE_CAPTURE_CHANGED:
      case ET_MOUSE_ENTERED:
      case ET_POINTER_CAPTURE_CHANGED:
      case ET_POINTER_ENTERED:
        return true;
      default:
        break;
    }
    return false;
  }

  // If |window| identifies an embedding and the owning client intercepts
  // events, this forwards to the owner and returns true. Otherwise returns
  // false.
  bool HandleInterceptedEvent(Event* event) {
    if (ShouldIgnoreEventType(event->type()))
      return false;

    // KeyEvents, and events when there is capture, do not go through through
    // ServerWindowTargeter. As a result ServerWindowEventHandler has to check
    // for a client intercepting events.
    if (server_window_->DoesOwnerInterceptEvents()) {
      server_window_->owning_window_tree()->SendEventToClient(window(), *event);
      event->StopPropagation();
      return true;
    }
    return false;
  }

 private:
  ServerWindow* const server_window_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowEventHandler);
};

class TopLevelEventHandler;

// PointerPressHandler is used to track state while a pointer is down.
// PointerPressHandler is typically destroyed when the pointer is released, but
// it may be destroyed at other times, such as when capture changes.
class PointerPressHandler : public aura::client::CaptureClientObserver,
                            public aura::WindowObserver {
 public:
  PointerPressHandler(TopLevelEventHandler* top_level_event_handler,
                      PointerId pointer_id,
                      const gfx::Point& location);
  ~PointerPressHandler() override;

  bool in_non_client_area() const { return in_non_client_area_; }

 private:
  // aura::client::CaptureClientObserver:
  void OnCaptureChanged(aura::Window* lost_capture,
                        aura::Window* gained_capture) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  TopLevelEventHandler* top_level_event_handler_;

  // True if the pointer down occurred in the non-client area.
  const bool in_non_client_area_;

  // Id of the pointer the handler was created for.
  const PointerId pointer_id_;

  DISALLOW_COPY_AND_ASSIGN(PointerPressHandler);
};

// ui::EventHandler used for top-levels. Some events that target the non-client
// area are not sent to the client, instead are handled locally. For example,
// if a press occurs in the non-client area, then the event is not sent to
// the client, it's handled locally.
class TopLevelEventHandler : public ServerWindowEventHandler {
 public:
  explicit TopLevelEventHandler(ServerWindow* server_window)
      : ServerWindowEventHandler(server_window) {
    // Top-levels should always have an owning_window_tree().
    // OnEvent() assumes this.
    DCHECK(server_window->owning_window_tree());
  }

  ~TopLevelEventHandler() override = default;

  void DestroyPointerPressHandler(PointerId id) {
    pointer_press_handlers_.erase(id);
  }

  // Returns true if the pointer with |pointer_id| was pressed over the
  // top-level. If this returns true, TopLevelEventHandler is waiting on a
  // release to reset state.
  bool IsHandlingPointerPress(PointerId pointer_id) const {
    return pointer_press_handlers_.count(pointer_id) > 0;
  }

  // Called when the capture owner changes.
  void OnCaptureOwnerChanged() {
    // Changing the capture owner toggles between local and the client getting
    // the event. The |pointer_press_handlers_| are no longer applicable
    // (because the target is purely dicatated by capture owner).
    pointer_press_handlers_.clear();
  }

  // ServerWindowEventHandler:
  void OnEvent(ui::Event* event) override {
    // This code doesn't handle PointerEvents, because they should never be
    // generated at this layer.
    DCHECK(!event->IsPointerEvent());

    if (event->phase() != EP_PRETARGET) {
      // All work is done in the pre-phase. If this branch is hit, it means
      // event propagation was not stopped, and normal processing should
      // continue. Early out to avoid sending the event to the client again.
      return;
    }

    if (HandleInterceptedEvent(event))
      return;

    if (!event->IsLocatedEvent()) {
      ServerWindowEventHandler::OnEvent(event);
      return;
    }

    // Gestures are always handled locally.
    if (ShouldIgnoreEvent(*event) || event->IsGestureEvent())
      return;

    // If there is capture, send the event to the client that owns it. A null
    // capture owner means the local environment should handle the event.
    if (wm::CaptureController::Get()->GetCaptureWindow()) {
      if (server_window()->capture_owner()) {
        server_window()->capture_owner()->SendEventToClient(window(), *event);
        event->StopPropagation();
        return;
      }
      return;
    }

    // This code does has two specific behaviors. It's used to ensure events
    // go to the right target (either local, or the remote client).
    // . a press-release sequence targets only one. If in non-client area then
    //   local, otherwise remote client.
    // . mouse-moves (not drags) go to both targets.
    bool stop_propagation = false;
    if (server_window()->HasNonClientArea() && IsPointerEvent(*event)) {
      const PointerId pointer_id = GetPointerId(*event);
      if (!pointer_press_handlers_.count(pointer_id)) {
        if (IsPointerPressedEvent(*event)) {
          std::unique_ptr<PointerPressHandler> handler_ptr =
              std::make_unique<PointerPressHandler>(
                  this, pointer_id, event->AsLocatedEvent()->location());
          PointerPressHandler* handler = handler_ptr.get();
          pointer_press_handlers_[pointer_id] = std::move(handler_ptr);
          if (handler->in_non_client_area())
            return;  // Don't send presses in non-client area to client.
          stop_propagation = true;
        }
      } else {
        // Currently handling a pointer press and waiting on release.
        PointerPressHandler* handler =
            pointer_press_handlers_[pointer_id].get();
        const bool was_press_in_non_client_area = handler->in_non_client_area();
        if (IsPointerReleased(*event))
          pointer_press_handlers_.erase(pointer_id);
        if (was_press_in_non_client_area)
          return;  // Don't send release to client since press didn't go there.
        stop_propagation = true;
      }
    }
    server_window()->owning_window_tree()->SendEventToClient(window(), *event);
    if (stop_propagation)
      event->StopPropagation();
  }

 private:
  // Non-null while in a pointer press press-drag-release cycle. Maps from
  // pointer-id of the pointer that is down to the handler.
  base::flat_map<PointerId, std::unique_ptr<PointerPressHandler>>
      pointer_press_handlers_;

  DISALLOW_COPY_AND_ASSIGN(TopLevelEventHandler);
};

PointerPressHandler::PointerPressHandler(
    TopLevelEventHandler* top_level_event_handler,
    PointerId pointer_id,
    const gfx::Point& location)
    : top_level_event_handler_(top_level_event_handler),
      in_non_client_area_(
          IsLocationInNonClientArea(top_level_event_handler->window(),
                                    location)),
      pointer_id_(pointer_id) {
  wm::CaptureController::Get()->AddObserver(this);
  top_level_event_handler_->window()->AddObserver(this);
}

PointerPressHandler::~PointerPressHandler() {
  top_level_event_handler_->window()->RemoveObserver(this);
  wm::CaptureController::Get()->RemoveObserver(this);
}

void PointerPressHandler::OnCaptureChanged(aura::Window* lost_capture,
                                           aura::Window* gained_capture) {
  if (gained_capture != top_level_event_handler_->window())
    top_level_event_handler_->DestroyPointerPressHandler(pointer_id_);
}

void PointerPressHandler::OnWindowVisibilityChanged(aura::Window* window,
                                                    bool visible) {
  if (!top_level_event_handler_->window()->IsVisible())
    top_level_event_handler_->DestroyPointerPressHandler(pointer_id_);
}

}  // namespace

ServerWindow::~ServerWindow() = default;

// static
ServerWindow* ServerWindow::Create(aura::Window* window,
                                   WindowTree* tree,
                                   const viz::FrameSinkId& frame_sink_id,
                                   bool is_top_level) {
  DCHECK(!GetMayBeNull(window));
  // Owned by |window|.
  ServerWindow* server_window =
      new ServerWindow(window, tree, frame_sink_id, is_top_level);
  return server_window;
}

// static
const ServerWindow* ServerWindow::GetMayBeNull(const aura::Window* window) {
  return window ? window->GetProperty(kServerWindowKey) : nullptr;
}

WindowTree* ServerWindow::embedded_window_tree() {
  return embedding_ ? embedding_->embedded_tree() : nullptr;
}

const WindowTree* ServerWindow::embedded_window_tree() const {
  return embedding_ ? embedding_->embedded_tree() : nullptr;
}

void ServerWindow::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  if (client_area_ == insets &&
      additional_client_areas == additional_client_areas_) {
    return;
  }

  additional_client_areas_ = additional_client_areas;
  client_area_ = insets;

  // TODO(sky): update cursor if over this window.
  NOTIMPLEMENTED_LOG_ONCE();
}

void ServerWindow::SetCaptureOwner(WindowTree* owner) {
  capture_owner_ = owner;
  if (!IsTopLevel())
    return;

  return static_cast<TopLevelEventHandler*>(event_handler_.get())
      ->OnCaptureOwnerChanged();
}

void ServerWindow::StoreCursor(const ui::Cursor& cursor) {
  cursor_ = cursor;
}

bool ServerWindow::DoesOwnerInterceptEvents() const {
  return embedding_ && embedding_->embedding_tree_intercepts_events();
}

void ServerWindow::SetEmbedding(std::unique_ptr<Embedding> embedding) {
  embedding_ = std::move(embedding);
}

bool ServerWindow::HasNonClientArea() const {
  return owning_window_tree_ && owning_window_tree_->IsTopLevel(window_) &&
         (!client_area_.IsEmpty() || !additional_client_areas_.empty());
}

bool ServerWindow::IsTopLevel() const {
  return owning_window_tree_ && owning_window_tree_->IsTopLevel(window_);
}

void ServerWindow::SetDragDropDelegate(
    std::unique_ptr<DragDropDelegate> drag_drop_delegate) {
  drag_drop_delegate_ = std::move(drag_drop_delegate);
}

void ServerWindow::AttachCompositorFrameSink(
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  host_frame_sink_manager->CreateCompositorFrameSink(
      frame_sink_id_, std::move(compositor_frame_sink), std::move(client));
}

ServerWindow::ServerWindow(aura::Window* window,
                           WindowTree* tree,
                           const viz::FrameSinkId& frame_sink_id,
                           bool is_top_level)
    : window_(window),
      owning_window_tree_(tree),
      frame_sink_id_(frame_sink_id) {
  window_->SetProperty(kServerWindowKey, this);
  if (is_top_level)
    event_handler_ = std::make_unique<TopLevelEventHandler>(this);
  else
    event_handler_ = std::make_unique<ServerWindowEventHandler>(this);
  window_->SetEventTargeter(std::make_unique<ServerWindowTargeter>(this));
  // In order for a window to receive events it must have a target_handler()
  // (see Window::CanAcceptEvent()). Normally the delegate is the TargetHandler,
  // but if the delegate is null, then so is the target_handler(). Set
  // |event_handler_| as the target_handler() to force the Window to accept
  // events.
  if (!window_->delegate())
    window_->SetTargetHandler(event_handler_.get());
}

bool ServerWindow::IsHandlingPointerPressForTesting(PointerId pointer_id) {
  DCHECK(IsTopLevel());
  return static_cast<TopLevelEventHandler*>(event_handler_.get())
      ->IsHandlingPointerPress(pointer_id);
}

}  // namespace ws2
}  // namespace ui
