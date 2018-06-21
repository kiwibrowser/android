// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_

#include <fuchsia/ui/input/cpp/fidl.h>
#include <fuchsia/ui/views_v1/cpp/fidl.h>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/platform/scenic/scenic_session.h"
#include "ui/platform_window/platform_window.h"

namespace ui {

class ScenicWindowManager;
class PlatformWindowDelegate;

class OZONE_EXPORT ScenicWindow : public PlatformWindow,
                                  public ScenicSessionListener,
                                  public fuchsia::ui::views_v1::ViewListener,
                                  public fuchsia::ui::input::InputListener {
 public:
  // Both |window_manager| and |delegate| must outlive the ScenicWindow.
  // |view_owner_request| is passed to the view managed when creating the
  // underlying view. In order for the View to be displayed the ViewOwner must
  // be used to add the view to a ViewContainer.
  ScenicWindow(ScenicWindowManager* window_manager,
               PlatformWindowDelegate* delegate,
               fidl::InterfaceRequest<fuchsia::ui::views_v1_token::ViewOwner>
                   view_owner_request);
  ~ScenicWindow() override;

  ScenicSession* scenic_session() { return &scenic_session_; }
  ScenicSession::ResourceId node_id() const { return node_id_; }
  float device_pixel_ratio() const { return device_pixel_ratio_; }

  // PlatformWindow implementation.
  gfx::Rect GetBounds() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetTitle(const base::string16& title) override;
  void Show() override;
  void Hide() override;
  void Close() override;
  void PrepareForShutdown() override;
  void SetCapture() override;
  void ReleaseCapture() override;
  bool HasCapture() const override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  PlatformWindowState GetPlatformWindowState() const override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;

 private:
  // views::ViewListener interface.
  void OnPropertiesChanged(fuchsia::ui::views_v1::ViewProperties properties,
                           OnPropertiesChangedCallback callback) override;

  // fuchsia::ui::input::InputListener interface.
  void OnEvent(fuchsia::ui::input::InputEvent event,
               OnEventCallback callback) override;

  // ScenicSessionListener interface.
  void OnScenicError(const std::string& error) override;
  void OnScenicEvents(
      const std::vector<fuchsia::ui::scenic::Event>& events) override;

  // Error handler for |view_|. This error normally indicates the View was
  // destroyed (e.g. dropping ViewOwner).
  void OnViewError();

  void UpdateSize();

  bool OnMouseEvent(const fuchsia::ui::input::PointerEvent& event);
  bool OnKeyboardEvent(const fuchsia::ui::input::KeyboardEvent& event);

  ScenicWindowManager* const manager_;
  PlatformWindowDelegate* const delegate_;
  gfx::AcceleratedWidget const window_id_;

  // Underlying View in the view_manager.
  fuchsia::ui::views_v1::ViewPtr view_;
  fidl::Binding<fuchsia::ui::views_v1::ViewListener> view_listener_binding_;

  // Scenic session used for all drawing operations in this View.
  ScenicSession scenic_session_;

  // Node ID in |scenic_session_| for the parent view.
  ScenicSession::ResourceId parent_node_id_;

  // Node ID in |scenic_session_| for the view.
  ScenicSession::ResourceId node_id_;

  // Current view size in DIPs.
  gfx::SizeF size_dips_;

  // Current view size in device pixels.
  gfx::Size size_pixels_;

  // Device pixel ratio for the current device. Initialized in
  // OnPropertiesChanged().
  float device_pixel_ratio_ = 0.0;

  // InputConnection and InputListener binding used to receive input events from
  // the view.
  fuchsia::ui::input::InputConnectionPtr input_connection_;
  fidl::Binding<fuchsia::ui::input::InputListener> input_listener_binding_;

  DISALLOW_COPY_AND_ASSIGN(ScenicWindow);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_WINDOW_H_
