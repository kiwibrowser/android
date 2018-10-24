// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/non_client_frame_controller.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/frame/custom_frame_view_ash.h"
#include "ash/frame/detached_title_area_renderer.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/move_event_handler.h"
#include "ash/wm/panels/panel_frame_view.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ash/ws/window_service_owner.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/ws2/window_properties.h"
#include "services/ui/ws2/window_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::NonClientFrameController*);

namespace ash {
namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(NonClientFrameController*,
                             kNonClientFrameControllerKey,
                             nullptr);

// This class supports draggable app windows that paint their own custom frames.
// It uses empty insets, doesn't paint anything, and hit tests return HTCAPTION.
class EmptyDraggableNonClientFrameView : public views::NonClientFrameView {
 public:
  EmptyDraggableNonClientFrameView() = default;
  ~EmptyDraggableNonClientFrameView() override = default;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTCAPTION; }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyDraggableNonClientFrameView);
};

// Creates a Window to host the top container when in immersive mode. The
// top container contains a DetachedTitleAreaRenderer, which handles drawing and
// events.
class ImmersiveFullscreenControllerDelegateMus
    : public ImmersiveFullscreenControllerDelegate {
 public:
  ImmersiveFullscreenControllerDelegateMus(views::Widget* frame,
                                           aura::Window* frame_window)
      : frame_(frame), frame_window_(frame_window) {}
  ~ImmersiveFullscreenControllerDelegateMus() override {
    DestroyTitleAreaWindow();
  }

  // WmImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override {
    CreateTitleAreaWindow();
    SetVisibleFraction(0);
  }
  void OnImmersiveRevealEnded() override { DestroyTitleAreaWindow(); }
  void OnImmersiveFullscreenEntered() override {}
  void OnImmersiveFullscreenExited() override { DestroyTitleAreaWindow(); }
  void SetVisibleFraction(double visible_fraction) override {
    aura::Window* title_area_window = GetTitleAreaWindow();
    if (!title_area_window)
      return;
    gfx::Rect bounds = title_area_window->bounds();
    bounds.set_y(frame_window_->bounds().y() - bounds.height() +
                 visible_fraction * bounds.height());
    title_area_window->SetBounds(bounds);
  }
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override {
    std::vector<gfx::Rect> result;
    const aura::Window* title_area_window = GetTitleAreaWindow();
    if (!title_area_window)
      return result;

    // Clip the bounds of the title area to that of the |frame_window_|.
    gfx::Rect visible_bounds = title_area_window->bounds();
    visible_bounds.Intersect(frame_window_->bounds());
    // The intersection is in the coordinates of |title_area_window|'s parent,
    // convert to be in |title_area_window| and then to screen.
    visible_bounds -= title_area_window->bounds().origin().OffsetFromOrigin();
    // TODO: this needs updating when parent of |title_area_window| is changed,
    // DCHECK is to ensure when parent changes this code is updated.
    // http://crbug.com/640392.
    DCHECK_EQ(frame_window_->parent(), title_area_window->parent());
    ::wm::ConvertRectToScreen(title_area_window, &visible_bounds);
    result.push_back(visible_bounds);
    return result;
  }

 private:
  void CreateTitleAreaWindow() {
    if (GetTitleAreaWindow())
      return;

    // TODO(sky): bounds aren't right here. Need to convert to display.
    gfx::Rect bounds = frame_window_->bounds();
    // Use the preferred size as when fullscreen the client area is generally
    // set to 0.
    bounds.set_height(
        NonClientFrameController::GetPreferredClientAreaInsets().top());
    bounds.set_y(bounds.y() - bounds.height());
    title_area_renderer_ =
        std::make_unique<DetachedTitleAreaRendererForInternal>(frame_);
    title_area_renderer_->widget()->SetBounds(bounds);
    title_area_renderer_->widget()->ShowInactive();
  }

  void DestroyTitleAreaWindow() { title_area_renderer_.reset(); }

  aura::Window* GetTitleAreaWindow() {
    return const_cast<aura::Window*>(
        const_cast<const ImmersiveFullscreenControllerDelegateMus*>(this)
            ->GetTitleAreaWindow());
  }
  const aura::Window* GetTitleAreaWindow() const {
    return title_area_renderer_
               ? title_area_renderer_->widget()->GetNativeView()
               : nullptr;
  }

  // The Widget immersive mode is operating on.
  views::Widget* frame_;

  // The ui::Window associated with |frame_|.
  aura::Window* frame_window_;

  std::unique_ptr<DetachedTitleAreaRendererForInternal> title_area_renderer_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenControllerDelegateMus);
};

class WmNativeWidgetAura : public views::NativeWidgetAura {
 public:
  WmNativeWidgetAura(views::internal::NativeWidgetDelegate* delegate,
                     aura::WindowManagerClient* window_manager_client,
                     bool remove_standard_frame,
                     bool enable_immersive,
                     mojom::WindowStyle window_style)
      // The NativeWidget is mirroring the real Widget created in client code.
      // |is_parallel_widget_in_window_manager| is used to indicate this
      : views::NativeWidgetAura(
            delegate,
            true /* is_parallel_widget_in_window_manager */),
        remove_standard_frame_(remove_standard_frame),
        enable_immersive_(enable_immersive),
        window_style_(window_style),
        window_manager_client_(window_manager_client) {}
  ~WmNativeWidgetAura() override = default;

  void SetHeaderHeight(int height) {
    if (custom_frame_view_)
      custom_frame_view_->SetHeaderHeight({height});
  }

  void set_cursor(const ui::Cursor& cursor) { cursor_ = cursor; }

  // views::NativeWidgetAura:
  views::NonClientFrameView* CreateNonClientFrameView() override {
    if (window_manager_client_) {
      move_event_handler_ = std::make_unique<MoveEventHandler>(
          window_manager_client_, GetNativeView());
    }
    // TODO(sky): investigate why we have this. Seems this should be the same
    // as not specifying client area insets.
    if (remove_standard_frame_)
      return new EmptyDraggableNonClientFrameView();
    aura::Window* window = GetNativeView();
    if (window->GetProperty(aura::client::kWindowTypeKey) ==
        ui::mojom::WindowType::PANEL)
      return new PanelFrameView(GetWidget(), PanelFrameView::FRAME_ASH);
    immersive_delegate_ =
        std::make_unique<ImmersiveFullscreenControllerDelegateMus>(GetWidget(),
                                                                   window);
    // See description for details on ownership.
    custom_frame_view_ =
        new CustomFrameViewAsh(GetWidget(), immersive_delegate_.get(),
                               enable_immersive_, window_style_);

    // Only the header actually paints any content. So the rest of the region is
    // marked as transparent content (see below in NonClientFrameController()
    // ctor). So, it is necessary to provide a texture-layer for the header
    // view.
    views::View* header_view = custom_frame_view_->GetHeaderView();
    header_view->SetPaintToLayer(ui::LAYER_TEXTURED);
    header_view->layer()->SetFillsBoundsOpaquely(false);

    return custom_frame_view_;
  }

  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return cursor_;
  }

 private:
  const bool remove_standard_frame_;
  const bool enable_immersive_;
  const mojom::WindowStyle window_style_;

  // TODO: this is no longer necessary once --mash is removed,
  // https://crbug.com/842365.
  std::unique_ptr<MoveEventHandler> move_event_handler_;

  aura::WindowManagerClient* window_manager_client_;

  std::unique_ptr<ImmersiveFullscreenControllerDelegateMus> immersive_delegate_;

  // Not used for panels or if |remove_standard_frame_| is true. This is owned
  // by the Widget's view hierarchy (e.g. it's a child of Widget's root View).
  CustomFrameViewAsh* custom_frame_view_ = nullptr;

  // The cursor for this widget. CompoundEventFilter will retrieve this cursor
  // via GetCursor and update the CursorManager's active cursor as appropriate
  // (i.e. when the mouse pointer is over this widget).
  ui::Cursor cursor_;

  DISALLOW_COPY_AND_ASSIGN(WmNativeWidgetAura);
};

class ClientViewMus : public views::ClientView {
 public:
  ClientViewMus(views::Widget* widget,
                views::View* contents_view,
                NonClientFrameController* frame_controller)
      : views::ClientView(widget, contents_view),
        frame_controller_(frame_controller) {}
  ~ClientViewMus() override = default;

  // views::ClientView:
  bool CanClose() override {
    // CanClose() is called when the user requests the window to close (such as
    // clicking the close button). As this window is managed by a remote client
    // pass the request to the remote client and return false (to cancel the
    // close). If the remote client wants the window to close, it will close it
    // in a way that does not reenter this code path.
    if (frame_controller_->window_manager_client()) {
      frame_controller_->window_manager_client()->RequestClose(
          frame_controller_->window());
    } else {
      Shell::Get()->window_service_owner()->window_service()->RequestClose(
          frame_controller_->window());
    }
    return false;
  }

 private:
  NonClientFrameController* frame_controller_;

  DISALLOW_COPY_AND_ASSIGN(ClientViewMus);
};

}  // namespace

NonClientFrameController::NonClientFrameController(
    aura::Window* parent,
    aura::Window* context,
    const gfx::Rect& bounds,
    ui::mojom::WindowType window_type,
    aura::PropertyConverter* property_converter,
    std::map<std::string, std::vector<uint8_t>>* properties,
    aura::WindowManagerClient* window_manager_client)
    : window_manager_client_(window_manager_client),
      widget_(new views::Widget),
      window_(nullptr) {
  // To simplify things this code creates a Widget. While a Widget is created
  // we need to ensure we don't inadvertently change random properties of the
  // underlying ui::Window. For example, showing the Widget shouldn't change
  // the bounds of the ui::Window in anyway.
  //
  // Assertions around InitParams::Type matching ui::mojom::WindowType exist in
  // MusClient.
  views::Widget::InitParams params(
      static_cast<views::Widget::InitParams::Type>(window_type));
  DCHECK((parent && !context) || (!parent && context));
  params.parent = parent;
  params.context = context;
  // TODO: properly set |params.activatable|. Should key off whether underlying
  // (mus) window can have focus.
  params.delegate = this;
  params.bounds = bounds;
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  params.layer_type = ui::LAYER_SOLID_COLOR;
  WmNativeWidgetAura* native_widget = new WmNativeWidgetAura(
      widget_, window_manager_client_, ShouldRemoveStandardFrame(*properties),
      ShouldEnableImmersive(*properties), GetWindowStyle(*properties));
  window_ = native_widget->GetNativeView();
  window_->SetProperty(aura::client::kEmbedType,
                       aura::client::WindowEmbedType::TOP_LEVEL_IN_WM);
  window_->SetProperty(kNonClientFrameControllerKey, this);
  window_->SetProperty(kWidgetCreationTypeKey, WidgetCreationType::FOR_CLIENT);
  window_->AddObserver(this);
  params.native_widget = native_widget;
  aura::SetWindowType(window_, window_type);
  for (auto& property_pair : *properties) {
    property_converter->SetPropertyFromTransportValue(
        window_, property_pair.first, &property_pair.second);
  }
  // Applying properties will have set the show state if specified.
  // NativeWidgetAura resets the show state from |params|, so we need to update
  // |params|.
  params.show_state = window_->GetProperty(aura::client::kShowStateKey);
  widget_->Init(params);
  did_init_native_widget_ = true;

  // Only the caption draws any content. So the caption has its own layer (see
  // above in WmNativeWidgetAura::CreateNonClientFrameView()). The rest of the
  // region needs to take part in occlusion in the compositor, but not generate
  // any content to draw. So the layer is marked as opaque and to draw
  // solid-color (but the color is transparent, so nothing is actually drawn).
  ui::Layer* layer = widget_->GetNativeWindow()->layer();
  layer->SetColor(SK_ColorTRANSPARENT);
  layer->SetFillsBoundsOpaquely(true);

  aura::client::GetTransientWindowClient()->AddObserver(this);
}

// static
NonClientFrameController* NonClientFrameController::Get(aura::Window* window) {
  return window->GetProperty(kNonClientFrameControllerKey);
}

// static
gfx::Insets NonClientFrameController::GetPreferredClientAreaInsets() {
  return gfx::Insets(
      GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption).height(), 0, 0, 0);
}

// static
int NonClientFrameController::GetMaxTitleBarButtonWidth() {
  return GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption).width() * 3;
}

void NonClientFrameController::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  client_area_insets_ = insets;
  additional_client_areas_ = additional_client_areas;
  static_cast<WmNativeWidgetAura*>(widget_->native_widget())
      ->SetHeaderHeight(insets.top());
}

void NonClientFrameController::StoreCursor(const ui::Cursor& cursor) {
  static_cast<WmNativeWidgetAura*>(widget_->native_widget())
      ->set_cursor(cursor);
}

NonClientFrameController::~NonClientFrameController() {
  aura::client::GetTransientWindowClient()->RemoveObserver(this);
  if (window_)
    window_->RemoveObserver(this);
}

base::string16 NonClientFrameController::GetWindowTitle() const {
  if (!window_ || !window_->GetProperty(aura::client::kTitleKey))
    return base::string16();

  base::string16 title = *window_->GetProperty(aura::client::kTitleKey);

  if (window_->GetProperty(kWindowIsJanky))
    title += base::ASCIIToUTF16(" !! Not responding !!");

  return title;
}

bool NonClientFrameController::CanResize() const {
  return window_ && (window_->GetProperty(aura::client::kResizeBehaviorKey) &
                     ui::mojom::kResizeBehaviorCanResize) != 0;
}

bool NonClientFrameController::CanMaximize() const {
  return window_ && (window_->GetProperty(aura::client::kResizeBehaviorKey) &
                     ui::mojom::kResizeBehaviorCanMaximize) != 0;
}

bool NonClientFrameController::CanMinimize() const {
  return window_ && (window_->GetProperty(aura::client::kResizeBehaviorKey) &
                     ui::mojom::kResizeBehaviorCanMinimize) != 0;
}

bool NonClientFrameController::CanActivate() const {
  // kCanFocus is used for both focus and activation.
  return window_ && window_->GetProperty(ui::ws2::kCanFocus) &&
         views::WidgetDelegate::CanActivate();
}

bool NonClientFrameController::ShouldShowWindowTitle() const {
  return window_ && window_->GetProperty(kWindowTitleShownKey);
}

views::ClientView* NonClientFrameController::CreateClientView(
    views::Widget* widget) {
  return new ClientViewMus(widget, GetContentsView(), this);
}

void NonClientFrameController::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  // Properties are applied before the call to InitNativeWidget(). Ignore
  // processing changes in this case as the Widget is not in a state where we
  // can use it yet.
  if (!did_init_native_widget_)
    return;

  if (key == kWindowIsJanky) {
    widget_->UpdateWindowTitle();
    widget_->non_client_view()->frame_view()->SchedulePaint();
  } else if (key == aura::client::kResizeBehaviorKey) {
    widget_->OnSizeConstraintsChanged();
  } else if (key == aura::client::kTitleKey) {
    widget_->UpdateWindowTitle();
  }
}

void NonClientFrameController::OnWindowDestroyed(aura::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

void NonClientFrameController::OnTransientChildWindowAdded(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_ ||
      !transient_child->GetProperty(kRenderTitleAreaProperty)) {
    return;
  }

  DetachedTitleAreaRendererForClient* renderer =
      DetachedTitleAreaRendererForClient::ForWindow(transient_child);
  if (!renderer || renderer->is_attached())
    return;

  renderer->Attach(widget_);
}

void NonClientFrameController::OnTransientChildWindowRemoved(
    aura::Window* parent,
    aura::Window* transient_child) {
  if (parent != window_)
    return;

  DetachedTitleAreaRendererForClient* renderer =
      DetachedTitleAreaRendererForClient::ForWindow(transient_child);
  if (renderer)
    renderer->Detach();
}

}  // namespace ash
