// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_window.h"

#include <string>

#include <fuchsia/sys/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

const uint32_t kUsbHidKeyboardPage = 0x07;

int KeyModifiersToFlags(int modifiers) {
  int flags = 0;
  if (modifiers & fuchsia::ui::input::kModifierShift)
    flags |= EF_SHIFT_DOWN;
  if (modifiers & fuchsia::ui::input::kModifierControl)
    flags |= EF_CONTROL_DOWN;
  if (modifiers & fuchsia::ui::input::kModifierAlt)
    flags |= EF_ALT_DOWN;
  // TODO(crbug.com/850697): Add AltGraph support.
  return flags;
}

}  // namespace

ScenicWindow::ScenicWindow(
    ScenicWindowManager* window_manager,
    PlatformWindowDelegate* delegate,
    fidl::InterfaceRequest<fuchsia::ui::views_v1_token::ViewOwner>
        view_owner_request)
    : manager_(window_manager),
      delegate_(delegate),
      window_id_(manager_->AddWindow(this)),
      view_listener_binding_(this),
      scenic_session_(manager_->GetScenic(), this),
      input_listener_binding_(this) {
  // Create event pair to import parent view node to Scenic. One end is passed
  // directly to Scenic in ImportResource command and the second one is passed
  // to ViewManager::CreateView(). ViewManager will passes it to Scenic when the
  // view is added to a container.
  zx::eventpair parent_export_token;
  zx::eventpair parent_import_token;
  zx_status_t status =
      zx::eventpair::create(0u, &parent_import_token, &parent_export_token);
  ZX_CHECK(status == ZX_OK, status) << "zx_eventpair_create()";

  // Create a new node and add it as a child to the parent.
  parent_node_id_ = scenic_session_.ImportResource(
      fuchsia::ui::gfx::ImportSpec::NODE, std::move(parent_import_token));
  node_id_ = scenic_session_.CreateEntityNode();
  scenic_session_.AddNodeChild(parent_node_id_, node_id_);

  // Subscribe to metrics events from the parent node. These events are used to
  // get |device_pixel_ratio_| for the screen.
  scenic_session_.SetEventMask(parent_node_id_,
                               fuchsia::ui::gfx::kMetricsEventMask);

  // Create the view.
  manager_->GetViewManager()->CreateView(
      view_.NewRequest(), std::move(view_owner_request),
      view_listener_binding_.NewBinding(), std::move(parent_export_token),
      "Chromium");
  view_.set_error_handler(fit::bind_member(this, &ScenicWindow::OnViewError));
  view_listener_binding_.set_error_handler(
      fit::bind_member(this, &ScenicWindow::OnViewError));

  // Setup input event listener.
  fuchsia::sys::ServiceProviderPtr view_service_provider;
  view_->GetServiceProvider(view_service_provider.NewRequest());
  view_service_provider->ConnectToService(
      fuchsia::ui::input::InputConnection::Name_,
      input_connection_.NewRequest().TakeChannel());
  input_connection_->SetEventListener(input_listener_binding_.NewBinding());

  // Call Present() to ensure that the scenic session commands are processed,
  // which is necessary to receive metrics event from Scenic.
  // OnAcceleratedWidgetAvailable() will be called after View metrics are
  // received.
  scenic_session_.Present();
}

ScenicWindow::~ScenicWindow() {
  delegate_->OnAcceleratedWidgetDestroying();

  scenic_session_.ReleaseResource(node_id_);
  scenic_session_.ReleaseResource(parent_node_id_);

  manager_->RemoveWindow(window_id_, this);
  view_.Unbind();

  delegate_->OnAcceleratedWidgetDestroyed();
}

gfx::Rect ScenicWindow::GetBounds() {
  return gfx::Rect(size_pixels_);
}

void ScenicWindow::SetBounds(const gfx::Rect& bounds) {
  // View dimensions are controlled by the containing view, it's not possible to
  // set them here.
}

void ScenicWindow::SetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void ScenicWindow::Show() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Hide() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Close() {
  NOTIMPLEMENTED();
}

void ScenicWindow::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void ScenicWindow::SetCapture() {
  NOTIMPLEMENTED();
}

void ScenicWindow::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool ScenicWindow::HasCapture() const {
  NOTIMPLEMENTED();
  return false;
}

void ScenicWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Maximize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Minimize() {
  NOTIMPLEMENTED();
}

void ScenicWindow::Restore() {
  NOTIMPLEMENTED();
}

PlatformWindowState ScenicWindow::GetPlatformWindowState() const {
  return PLATFORM_WINDOW_STATE_NORMAL;
}

void ScenicWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void ScenicWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void ScenicWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* ScenicWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ScenicWindow::UpdateSize() {
  gfx::SizeF scaled = ScaleSize(size_dips_, device_pixel_ratio_);
  size_pixels_ = gfx::Size(ceilf(scaled.width()), ceilf(scaled.height()));
  delegate_->OnBoundsChanged(gfx::Rect(size_pixels_));
}

void ScenicWindow::OnPropertiesChanged(
    fuchsia::ui::views_v1::ViewProperties properties,
    OnPropertiesChangedCallback callback) {
  if (properties.view_layout) {
    size_dips_.SetSize(properties.view_layout->size.width,
                       properties.view_layout->size.height);
    if (device_pixel_ratio_ > 0.0)
      UpdateSize();
  }

  callback();
}

void ScenicWindow::OnScenicError(const std::string& error) {
  LOG(ERROR) << "ScenicSession failed: " << error;
  delegate_->OnClosed();
}

void ScenicWindow::OnScenicEvents(
    const std::vector<fuchsia::ui::scenic::Event>& events) {
  for (const auto& event : events) {
    if (!event.is_gfx() || !event.gfx().is_metrics())
      continue;

    auto& metrics = event.gfx().metrics();
    if (metrics.node_id == parent_node_id_) {
      float new_device_pixel_ratio =
          std::max(metrics.metrics.scale_x, metrics.metrics.scale_y);
      if (device_pixel_ratio_ == 0.0) {
        device_pixel_ratio_ = new_device_pixel_ratio;
        delegate_->OnAcceleratedWidgetAvailable(window_id_,
                                                device_pixel_ratio_);
        if (!size_dips_.IsEmpty())
          UpdateSize();
      } else if (device_pixel_ratio_ != new_device_pixel_ratio) {
        // Ozone currently doesn't support dynamic changes in
        // device_pixel_ratio.
        // TODO(crbug.com/850650): Update Ozone/Aura to allow DPI changes
        // after OnAcceleratedWidgetAvailable().
        NOTIMPLEMENTED() << "Ignoring display metrics event.";
      }
    }
  }
}

void ScenicWindow::OnEvent(fuchsia::ui::input::InputEvent event,
                           OnEventCallback callback) {
  bool result = false;

  switch (event.Which()) {
    case fuchsia::ui::input::InputEvent::Tag::kPointer:
      // TODO(crbug.com/829980): Add touch support.
      if (event.pointer().type == fuchsia::ui::input::PointerEventType::MOUSE)
        result = OnMouseEvent(event.pointer());
      break;

    case fuchsia::ui::input::InputEvent::Tag::kKeyboard:
      result = OnKeyboardEvent(event.keyboard());
      break;

    case fuchsia::ui::input::InputEvent::Tag::kFocus:
    case fuchsia::ui::input::InputEvent::Tag::Invalid:
      break;
  }

  callback(result);
}

void ScenicWindow::OnViewError() {
  VLOG(1) << "views_v1::View connection was closed.";
  delegate_->OnClosed();
}

bool ScenicWindow::OnMouseEvent(const fuchsia::ui::input::PointerEvent& event) {
  int flags = 0;
  if (event.buttons & 1)
    flags |= EF_LEFT_MOUSE_BUTTON;
  if (event.buttons & 2)
    flags |= EF_RIGHT_MOUSE_BUTTON;
  if (event.buttons & 4)
    flags |= EF_MIDDLE_MOUSE_BUTTON;

  EventType event_type;

  switch (event.phase) {
    case fuchsia::ui::input::PointerEventPhase::DOWN:
      event_type = ET_MOUSE_PRESSED;
      break;
    case fuchsia::ui::input::PointerEventPhase::MOVE:
      event_type = flags ? ET_MOUSE_DRAGGED : ET_MOUSE_MOVED;
      break;
    case fuchsia::ui::input::PointerEventPhase::UP:
      event_type = ET_MOUSE_RELEASED;
      break;

    // Following phases are not expected for mouse events.
    case fuchsia::ui::input::PointerEventPhase::HOVER:
    case fuchsia::ui::input::PointerEventPhase::CANCEL:
    case fuchsia::ui::input::PointerEventPhase::ADD:
    case fuchsia::ui::input::PointerEventPhase::REMOVE:
      NOTREACHED() << "Unexpected mouse phase " << event.phase;
      return false;
  }

  gfx::Point location =
      gfx::Point(event.x * device_pixel_ratio_, event.y * device_pixel_ratio_);
  ui::MouseEvent mouse_event(event_type, location, location,
                             base::TimeTicks::FromZxTime(event.event_time),
                             flags, 0);
  delegate_->DispatchEvent(&mouse_event);
  return true;
}

bool ScenicWindow::OnKeyboardEvent(
    const fuchsia::ui::input::KeyboardEvent& event) {
  EventType event_type;

  switch (event.phase) {
    case fuchsia::ui::input::KeyboardEventPhase::PRESSED:
    case fuchsia::ui::input::KeyboardEventPhase::REPEAT:
      event_type = ET_KEY_PRESSED;
      break;

    case fuchsia::ui::input::KeyboardEventPhase::RELEASED:
      event_type = ET_KEY_RELEASED;
      break;

    case fuchsia::ui::input::KeyboardEventPhase::CANCELLED:
      NOTIMPLEMENTED() << "Key event cancellation is not supported.";
      event_type = ET_KEY_RELEASED;
      break;
  }

  // Currently KeyboardEvent doesn't specify HID Usage page. |hid_usage|
  // field always contains values from the Keyboard page. See
  // https://fuchsia.atlassian.net/browse/SCN-762 .
  DomCode dom_code = KeycodeConverter::UsbKeycodeToDomCode(
      (kUsbHidKeyboardPage << 16) | event.hid_usage);
  DomKey dom_key;
  KeyboardCode key_code;
  if (!DomCodeToUsLayoutDomKey(dom_code, KeyModifiersToFlags(event.modifiers),
                               &dom_key, &key_code)) {
    LOG(ERROR) << "DomCodeToUsLayoutDomKey() failed for usb_key: "
               << event.hid_usage;
    key_code = VKEY_UNKNOWN;
  }

  if (event.code_point)
    dom_key = DomKey::FromCharacter(event.code_point);

  KeyEvent key_event(ET_KEY_PRESSED, key_code, dom_code,
                     KeyModifiersToFlags(event.modifiers), dom_key,
                     base::TimeTicks::FromZxTime(event.event_time));
  delegate_->DispatchEvent(&key_event);
  return true;
}

}  // namespace ui
