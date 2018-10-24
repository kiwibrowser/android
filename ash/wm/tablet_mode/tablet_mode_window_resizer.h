// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_resizer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

namespace wm {
class WindowState;
}  // namespace wm

class SplitViewDragIndicators;

// WindowResizer implementation for windows in tablet mode. Currently we don't
// allow any resizing and any dragging happening on the area other than the
// caption tabs area in tablet mode. Only browser windows with tabs are allowed
// to be dragged. Depending on the event position, the dragged window may be 1)
// maximized, or 2) snapped in splitscreen, or 3) merged to an existing window.
class TabletModeWindowResizer : public WindowResizer {
 public:
  explicit TabletModeWindowResizer(wm::WindowState* window_state);
  ~TabletModeWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::Point& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;

  SplitViewDragIndicators* split_view_drag_indicators_for_testing() {
    return split_view_drag_indicators_.get();
  }

 private:
  enum class EndDragType {
    NORMAL,
    REVERT,
  };
  void EndDragImpl(EndDragType type);

  // Updates the split view drag indicators and preview window according to the
  // current location |location_in_screen|.
  void UpdateIndicatorsAndPreviewWindow(const gfx::Point& location_in_screen);

  // Scales down the source window if the dragged window is dragged past the
  // |kIndicatorThresholdRatio| threshold and restores it if the dragged window
  // is dragged back toward the top of the screen. |location_in_screen| is the
  // current drag location for the dragged window.
  void UpdateSourceWindow(const gfx::Point& location_in_screen);

  // Shows/Hides/Destroies the scrim widget |scrim_| based on the current
  // location |location_in_screen|.
  void UpdateScrim(const gfx::Point& location_in_screen);

  // Gets the desired snap position for |location_in_screen|.
  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  // Shows the scrim with the specified opacity, blur and expected bounds.
  void ShowScrim(float opacity, float blur, const gfx::Rect& bounds_in_screen);

  // Returns true if the drag indicators should show.
  bool ShouldShowDragIndicators(const gfx::Point& location_in_screen) const;

  SplitViewController* split_view_controller_;

  // A widget placed below the current dragged window to show the blurred or
  // transparent background and to prevent the dragged window merge into any
  // browser window beneath it during dragging.
  std::unique_ptr<views::Widget> scrim_;

  // A widget to display the drag indicators and preview window.
  std::unique_ptr<SplitViewDragIndicators> split_view_drag_indicators_;

  gfx::Point previous_location_in_parent_;

  bool did_lock_cursor_ = false;

  // The backdrop should be disabled during dragging and resumed after dragging.
  BackdropWindowMode original_backdrop_mode_;

  // Used to determine if this has been deleted during a drag such as when a tab
  // gets dragged into another browser window.
  base::WeakPtrFactory<TabletModeWindowResizer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowResizer);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_RESIZER_H_
