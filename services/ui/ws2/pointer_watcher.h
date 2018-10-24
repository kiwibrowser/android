// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_POINTER_WATCHER_H_
#define SERVICES_UI_WS2_POINTER_WATCHER_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_event_dispatcher_observer.h"

namespace ui {

class Event;

namespace ws2 {

class WindowTree;

// PointerWatcher is used when a client has requested to observe pointer events
// that the client would not normally receive. PointerWatcher observes events
// by way of aura::WindowEventDispatcherObserver and forwards them to the
// client.
//
// This class provides the server implementation of
// ui::mojom::WindowTree::StartPointerWatcher(), see it for more information.
class PointerWatcher : public aura::WindowEventDispatcherObserver {
 public:
  enum class TypesToWatch {
    // Pointer up/down events.
    kUpDown,

    // Pointer up, down, move (including drag) and wheel events.
    kUpDownMoveWheel,
  };

  explicit PointerWatcher(WindowTree* tree);
  ~PointerWatcher() override;

  // Applies any necessary transformations on the event before sending to the
  // client.
  static std::unique_ptr<Event> CreateEventForClient(const Event& event);

  // Returns true if |event| matches the types the PointerWatcher has been
  // configured to monitor.
  bool DoesEventMatch(const ui::Event& event) const;

  void set_types_to_watch(TypesToWatch types) { types_to_watch_ = types; }

  // See comment above |pending_event_| for details.
  void ClearPendingEvent();

 private:
  // aura::WindowEventDispatcherObserver:
  void OnWindowEventDispatcherStartedProcessing(
      aura::WindowEventDispatcher* dispatcher,
      const ui::Event& event) override;
  void OnWindowEventDispatcherFinishedProcessingEvent(
      aura::WindowEventDispatcher* dispatcher) override;

  TypesToWatch types_to_watch_ = TypesToWatch::kUpDown;

  WindowTree* tree_;

  // Events matching TypesToWatch are processed in two phases:
  // . In OnWindowEventDispatcherStartedProcessing() if the event should be
  //   sent to the client, it's stored in |pending_event_|.
  // . In OnWindowEventDispatcherFinishedProcessingEvent() if |pending_event_|
  //   is non-null, |pending_event_| is sent to the client.
  // During event processing if the event targets the client, then
  // |pending_event_| is reset. This is done to avoid sending the event twice.
  // WindowTreeClient::OnWindowInputEvent() indicates if the event matched
  // an observed pointer event.
  std::unique_ptr<ui::Event> pending_event_;

  DISALLOW_COPY_AND_ASSIGN(PointerWatcher);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_POINTER_WATCHER_H_
