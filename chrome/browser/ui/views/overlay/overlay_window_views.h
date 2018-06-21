// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_

#include "content/public/browser/overlay_window.h"

#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace views {
class ImageButton;
class ToggleImageButton;
}  // namespace views

// The Chrome desktop implementation of OverlayWindow. This will only be
// implemented in views, which will support all desktop platforms.
class OverlayWindowViews : public content::OverlayWindow, public views::Widget {
 public:
  // The list of control buttons that appear on the window.
  enum ControlButton {
    CONTROL_PLAY_PAUSE,
    CONTROL_CLOSE,
  };

  explicit OverlayWindowViews(
      content::PictureInPictureWindowController* controller);
  ~OverlayWindowViews() override;

  // OverlayWindow:
  bool IsActive() const override;
  void Close() override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  bool IsAlwaysOnTop() const override;
  ui::Layer* GetLayer() override;
  gfx::Rect GetBounds() const override;
  void UpdateVideoSize(const gfx::Size& natural_size) override;
  void UpdatePlayPauseControlsIcon(bool is_playing) override;
  ui::Layer* GetVideoLayer() override;
  ui::Layer* GetControlsBackgroundLayer() override;
  ui::Layer* GetCloseControlsLayer() override;
  ui::Layer* GetPlayPauseControlsLayer() override;
  gfx::Rect GetVideoBounds() override;
  gfx::Rect GetCloseControlsBounds() override;
  gfx::Rect GetPlayPauseControlsBounds() override;

  // views::Widget:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::internal::NativeWidgetDelegate:
  void OnNativeFocus() override;
  void OnNativeBlur() override;
  void OnNativeWidgetSizeChanged(const gfx::Size& new_size) override;

 private:
  // Determine the intended bounds of |this|. This should be called when there
  // is reason for the bounds to change, such as switching primary displays or
  // playing a new video (i.e. different aspect ratio). This also updates
  // |min_size_| and |max_size_|.
  gfx::Rect CalculateAndUpdateWindowBounds();

  // Set up the views::Views that will be shown on the window.
  void SetUpViews();

  // Update |video_bounds_| to fit within |window_bounds_| while adhering to
  // the aspect ratio of the video, which is retrieved from |natural_size_|.
  void UpdateVideoLayerSizeWithAspectRatio(gfx::Size window_size);

  // Updates the controls view::Views to reflect |is_visible|.
  void UpdateControlsVisibility(bool is_visible);

  // Update the size of |close_controls_view_| as the size of the window
  // changes. This will scale to one of three sizes, based off the current width
  // of the window.
  void UpdateCloseControlsSize();

  // Update the size of |play_pause_controls_view_| as the size of the window
  // changes.
  void UpdatePlayPauseControlsSize();

  // Toggles the play/pause control through the |controller_| and updates the
  // |play_pause_controls_view_| toggled state to reflect the current playing
  // state.
  void TogglePlayPause();

  // Not owned; |controller_| owns |this|.
  content::PictureInPictureWindowController* controller_;

  // Whether or not the components of the window has been set up. This is used
  // as a check as some event handlers (e.g. focus) is propogated to the window
  // before its contents is initialized. This is only set once.
  bool is_initialized_ = false;

  // Whether or not the controls of the window should be shown. This is used in
  // some event handlers (e.g. focus).
  bool should_show_controls_ = false;

  // The upper and lower bounds of |current_size_|. These are determined by the
  // size of the primary display work area when Picture-in-Picture is initiated.
  // TODO(apacible): Update these bounds when the display the window is on
  // changes. http://crbug.com/819673
  gfx::Size min_size_;
  gfx::Size max_size_;

  // Current sizes of |close_controls_view_| and |play_pause_controls_view_|.
  gfx::Size close_button_size_;
  gfx::Size play_pause_button_size_;

  // Current bounds of the Picture-in-Picture window.
  gfx::Rect window_bounds_;

  // Bounds of |video_view_|.
  gfx::Rect video_bounds_;

  // The natural size of the video to show. This is used to compute sizing and
  // ensuring factors such as aspect ratio is maintained.
  gfx::Size natural_size_;

  // The currently focused button on the window. This is used for keeping
  // track of focus on the window while tabbing.
  ControlButton focused_control_button_;

  // Views to be shown.
  std::unique_ptr<views::View> video_view_;
  std::unique_ptr<views::View> controls_background_view_;
  std::unique_ptr<views::ImageButton> close_controls_view_;
  std::unique_ptr<views::ToggleImageButton> play_pause_controls_view_;

  DISALLOW_COPY_AND_ASSIGN(OverlayWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_OVERLAY_WINDOW_VIEWS_H_
