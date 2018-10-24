// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_resizer.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/wm/drag_details.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/root_window_finder.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The threshold to compute the minimum vertical distance to start showing the
// drag indicators and preview window.
constexpr float kIndicatorsThresholdRatio = 0.1;

// The threshold to compute the vertical distance to hide the drag indicators
// and maximize the dragged window after the drag ends.
constexpr float kMaximizeThresholdRatio = 0.4;

// The scale factor that the source window should scale if the source window is
// not the dragge window && is not in splitscreen when drag starts && the user
// has dragged the window to pass the |kIndicatorThresholdRatio| vertical
// threshold.
constexpr float kSourceWindowScale = 0.85;

// Values of the scrim.
constexpr SkColor kScrimBackgroundColor = SK_ColorGRAY;
constexpr float kScrimOpacity = 0.8;
constexpr float kScrimBlur = 5.f;
constexpr int kScrimTransitionInMs = 250;
constexpr int kScrimRoundRectRadiusDp = 4;

// Returns the window selector if overview mode is active, otherwise returns
// nullptr.
WindowSelector* GetWindowSelector() {
  return Shell::Get()->window_selector_controller()->IsSelecting()
             ? Shell::Get()->window_selector_controller()->window_selector()
             : nullptr;
}

// Returns the window selector item in overview that contains the specified
// location. Returns nullptr if there is no such window selector item.
WindowSelectorItem* GetWindowSelectorItemContains(
    const gfx::Point& location_in_screen) {
  if (!Shell::Get()->window_selector_controller()->IsSelecting())
    return nullptr;

  WindowGrid* current_grid = GetWindowSelector()->GetGridWithRootWindow(
      wm::GetRootWindowAt(location_in_screen));
  if (!current_grid)
    return nullptr;

  const auto& windows = current_grid->window_list();
  for (const auto& window_selector_item : windows) {
    if (window_selector_item->target_bounds().Contains(location_in_screen))
      return window_selector_item.get();
  }
  return nullptr;
}

// Creates a transparent scrim which is placed below |dragged_window|.
std::unique_ptr<views::Widget> CreateScrim(aura::Window* dragged_window,
                                           const gfx::Rect& bounds) {
  std::unique_ptr<views::Widget> widget = CreateBackgroundWidget(
      /*root_window=*/dragged_window->GetRootWindow(),
      /*layer_type=*/ui::LAYER_TEXTURED,
      /*background_color=*/kScrimBackgroundColor,
      /*border_thickness=*/0,
      /*border_radius=*/kScrimRoundRectRadiusDp,
      /*border_color=*/SK_ColorTRANSPARENT,
      /*initial_opacity=*/0.f,
      /*parent=*/dragged_window->parent(),
      /*stack_on_top=*/false);
  widget->SetBounds(bounds);
  return widget;
}

// When the dragged window is dragged past this value, the drag indicators will
// show up.
int GetIndicatorsVerticalThreshold(const gfx::Rect& work_area_bounds) {
  return work_area_bounds.y() +
         work_area_bounds.height() * kIndicatorsThresholdRatio;
}

// When the dragged window is dragged past this value, a scrim will show up,
// indicating the dragged window will be maximized after releasing.
int GetMaximizeVerticalThreshold(const gfx::Rect& work_area_bounds) {
  return work_area_bounds.y() +
         work_area_bounds.height() * kMaximizeThresholdRatio;
}

}  // namespace

TabletModeWindowResizer::TabletModeWindowResizer(wm::WindowState* window_state)
    : WindowResizer(window_state),
      split_view_controller_(Shell::Get()->split_view_controller()),
      weak_ptr_factory_(this) {
  DCHECK(details().is_resizable);

  if (details().source != ::wm::WINDOW_MOVE_SOURCE_TOUCH &&
      !window_state->allow_set_bounds_direct()) {
    ShellPort::Get()->LockCursor();
    did_lock_cursor_ = true;
  }

  previous_location_in_parent_ = details().initial_location_in_parent;
  window_state_->OnDragStarted(details().window_component);

  // Disable the backdrop on the dragged window.
  original_backdrop_mode_ = GetTarget()->GetProperty(kBackdropWindowMode);
  GetTarget()->SetProperty(kBackdropWindowMode, BackdropWindowMode::kDisabled);
  split_view_controller_->OnWindowDragStarted(GetTarget());

  // If the source window itself is the dragged window (i.e., all the source
  // window's tabs are dragged together when drag starts), open overview behind
  // the dragged window.
  aura::Window* source_window =
      GetTarget()->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (!source_window &&
      !Shell::Get()->window_selector_controller()->IsSelecting()) {
    Shell::Get()->window_selector_controller()->ToggleOverview();
  }

  if (GetWindowSelector())
    GetWindowSelector()->OnWindowDragStarted(GetTarget());

  split_view_drag_indicators_ = std::make_unique<SplitViewDragIndicators>();
}

TabletModeWindowResizer::~TabletModeWindowResizer() {
  if (did_lock_cursor_)
    ShellPort::Get()->UnlockCursor();
}

void TabletModeWindowResizer::Drag(const gfx::Point& location_in_parent,
                                   int event_flags) {
  gfx::Point location_in_screen = location_in_parent;
  ::wm::ConvertPointToScreen(GetTarget()->parent(), &location_in_screen);

  // Update the drag indicators and snap preview window if necessary.
  UpdateIndicatorsAndPreviewWindow(location_in_screen);

  // Update the source window if necessary.
  UpdateSourceWindow(location_in_screen);

  // Update the scrim that beneath the dragged window if necessary.
  UpdateScrim(location_in_screen);

  // Update dragged window's bounds.
  gfx::Rect bounds = CalculateBoundsForDrag(location_in_parent);
  if (bounds != GetTarget()->bounds()) {
    base::WeakPtr<TabletModeWindowResizer> resizer(
        weak_ptr_factory_.GetWeakPtr());
    GetTarget()->SetBounds(bounds);
    if (!resizer)
      return;
  }

  previous_location_in_parent_ = location_in_parent;

  if (GetWindowSelector())
    GetWindowSelector()->OnWindowDragContinued(GetTarget(), location_in_screen);
}

void TabletModeWindowResizer::CompleteDrag() {
  EndDragImpl(EndDragType::NORMAL);
}

void TabletModeWindowResizer::RevertDrag() {
  EndDragImpl(EndDragType::REVERT);
}

void TabletModeWindowResizer::EndDragImpl(EndDragType type) {
  gfx::Point previous_location_in_screen = previous_location_in_parent_;
  ::wm::ConvertPointToScreen(GetTarget()->parent(),
                             &previous_location_in_screen);
  if (type == EndDragType::NORMAL)
    window_state_->OnCompleteDrag(previous_location_in_screen);
  else
    window_state_->OnRevertDrag(previous_location_in_screen);
  GetTarget()->SetProperty(kBackdropWindowMode, original_backdrop_mode_);

  // The window might merge into an overview window or become a new window item
  // in overview mode.
  if (GetWindowSelector()) {
    GetWindowSelector()->OnWindowDragEnded(GetTarget(),
                                           previous_location_in_screen);
  }

  // At this moment we could not decide what might happen to the dragged window.
  // It can either 1) be a new window or 2) be destoryed due to attaching into
  // another browser window. We should avoid to snap a to-be-destroyed window.
  // Start observing it until we can decide what to do next.
  split_view_controller_->OnWindowDragEnded(
      GetTarget(),
      type == EndDragType::NORMAL ? GetSnapPosition(previous_location_in_screen)
                                  : SplitViewController::NONE,
      previous_location_in_screen);

  // The source window might have been scaled during dragging, update its bounds
  // to ensure it has the right bounds after the drag ends.
  aura::Window* source_window =
      GetTarget()->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (source_window && !wm::GetWindowState(source_window)->IsSnapped()) {
    TabletModeWindowState::UpdateWindowPosition(
        wm::GetWindowState(source_window));
  }
}

void TabletModeWindowResizer::UpdateIndicatorsAndPreviewWindow(
    const gfx::Point& location_in_screen) {
  if (GetSnapPosition(location_in_screen) != SplitViewController::NONE &&
      !split_view_controller_->IsSplitViewModeActive()) {
    // Show the preview window if |location_in_screen| is not contained by an
    // eligible target window item to merge the dragged window.
    WindowSelectorItem* item =
        GetWindowSelectorItemContains(location_in_screen);
    if (!item || !item->GetWindow()->GetProperty(
                     ash::kIsDeferredTabDraggingTargetWindowKey)) {
      split_view_drag_indicators_->SetIndicatorState(
          GetSnapPosition(location_in_screen) == SplitViewController::LEFT
              ? IndicatorState::kPreviewAreaLeft
              : IndicatorState::kPreviewAreaRight,
          location_in_screen);
    }
    return;
  }

  split_view_drag_indicators_->SetIndicatorState(
      ShouldShowDragIndicators(location_in_screen) ? IndicatorState::kDragArea
                                                   : IndicatorState::kNone,
      location_in_screen);
}

void TabletModeWindowResizer::UpdateSourceWindow(
    const gfx::Point& location_in_screen) {
  // Only do the scale if the source window is not the dragged window && the
  // source window is not in splitscreen && the source window is not in
  // overview.
  aura::Window* source_window =
      GetTarget()->GetProperty(ash::kTabDraggingSourceWindowKey);
  if (!source_window || source_window == GetTarget() ||
      source_window == split_view_controller_->left_window() ||
      source_window == split_view_controller_->right_window() ||
      (GetWindowSelector() &&
       GetWindowSelector()->IsWindowInOverview(source_window))) {
    return;
  }

  const gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(GetTarget())
                                         .work_area();
  gfx::Rect expected_bounds(work_area_bounds);
  if (location_in_screen.y() >=
      GetIndicatorsVerticalThreshold(work_area_bounds)) {
    SplitViewController::SnapPosition snap_position =
        GetSnapPosition(location_in_screen);

    if (snap_position == SplitViewController::NONE) {
      // Scale down the source window if the event location passes the vertical
      // |kIndicatorThresholdRatio| threshold.
      expected_bounds.ClampToCenteredSize(
          gfx::Size(work_area_bounds.width() * kSourceWindowScale,
                    work_area_bounds.height() * kSourceWindowScale));
    } else {
      // Put the source window on the other side of the split screen.
      expected_bounds = split_view_controller_->GetSnappedWindowBoundsInScreen(
          source_window, snap_position == SplitViewController::LEFT
                             ? SplitViewController::RIGHT
                             : SplitViewController::LEFT);
    }
  }

  source_window->SetBoundsInScreen(
      expected_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(source_window));
}

void TabletModeWindowResizer::UpdateScrim(
    const gfx::Point& location_in_screen) {
  const gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(GetTarget())
                                         .work_area();
  if (location_in_screen.y() <
      GetIndicatorsVerticalThreshold(work_area_bounds)) {
    // Remove |scrim_| entirely so that the dragged window can be merged back
    // to the source window when the dragged window is dragged back toward the
    // top area of the screen.
    scrim_.reset();
    return;
  }

  // If overview mode is active, do not show the scrim on the overview side of
  // the screen.
  if (Shell::Get()->window_selector_controller()->IsSelecting()) {
    WindowGrid* window_grid = GetWindowSelector()->GetGridWithRootWindow(
        GetTarget()->GetRootWindow());
    if (window_grid && window_grid->bounds().Contains(location_in_screen)) {
      scrim_.reset();
      return;
    }
  }

  SplitViewController::SnapPosition snap_position =
      GetSnapPosition(location_in_screen);
  gfx::Rect expected_bounds(work_area_bounds);
  if (split_view_controller_->IsSplitViewModeActive()) {
    expected_bounds = split_view_controller_->GetSnappedWindowBoundsInScreen(
        GetTarget(), snap_position);
  } else {
    expected_bounds.Inset(kHighlightScreenEdgePaddingDp,
                          kHighlightScreenEdgePaddingDp);
  }

  bool should_show_blurred_scrim = false;
  if (location_in_screen.y() >=
      GetMaximizeVerticalThreshold(work_area_bounds)) {
    if (split_view_controller_->IsSplitViewModeActive() !=
        (snap_position == SplitViewController::NONE)) {
      should_show_blurred_scrim = true;
    }
  }
  // When the event is between |indicators_vertical_threshold| and
  // |maximize_vertical_threshold|, the scrim is still shown but is invisible
  // to the user (transparent). It's needed to prevent the dragged window to
  // merge into the scaled down source window.
  ShowScrim(should_show_blurred_scrim ? kScrimOpacity : 0.f,
            should_show_blurred_scrim ? kScrimBlur : 0.f, expected_bounds);
}

SplitViewController::SnapPosition TabletModeWindowResizer::GetSnapPosition(
    const gfx::Point& location_in_screen) const {
  gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(GetTarget())
                                   .work_area();

  // The user has to drag pass the indicator vertical threshold to snap the
  // window.
  if (location_in_screen.y() < GetIndicatorsVerticalThreshold(work_area_bounds))
    return SplitViewController::NONE;

  const bool is_landscape =
      split_view_controller_->IsCurrentScreenOrientationLandscape();
  const bool is_primary =
      split_view_controller_->IsCurrentScreenOrientationPrimary();

  // If split view mode is active during dragging, the dragged window will be
  // either snapped left or right (if it's not merged into overview window),
  // depending on the relative position of |location_in_screen| and the current
  // divider position.
  if (split_view_controller_->IsSplitViewModeActive()) {
    const int position =
        is_landscape ? location_in_screen.x() : location_in_screen.y();
    if (position < split_view_controller_->divider_position()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    } else {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
  }

  // Otherwise, check to see if the current event location |location_in_screen|
  // is within the drag indicators bounds.
  if (is_landscape) {
    const int screen_edge_inset =
        work_area_bounds.width() * kHighlightScreenPrimaryAxisRatio +
        kHighlightScreenEdgePaddingDp;
    work_area_bounds.Inset(screen_edge_inset, 0);
    if (location_in_screen.x() < work_area_bounds.x()) {
      return is_primary ? SplitViewController::LEFT
                        : SplitViewController::RIGHT;
    }
    if (location_in_screen.x() >= work_area_bounds.right()) {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
    return SplitViewController::NONE;
  } else {
    // For portrait mode, since the drag always starts from the top of the
    // screen, we only allow the window to be dragged to snap to the bottom of
    // the screen.
    const int screen_edge_inset =
        work_area_bounds.height() * kHighlightScreenPrimaryAxisRatio +
        kHighlightScreenEdgePaddingDp;
    work_area_bounds.Inset(0, screen_edge_inset);
    if (location_in_screen.y() >= work_area_bounds.bottom()) {
      return is_primary ? SplitViewController::RIGHT
                        : SplitViewController::LEFT;
    }
    return SplitViewController::NONE;
  }
}

void TabletModeWindowResizer::ShowScrim(float opacity,
                                        float blur,
                                        const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds(bounds_in_screen);
  ::wm::ConvertRectFromScreen(GetTarget()->parent(), &bounds);

  if (scrim_ && scrim_->GetLayer()->GetTargetOpacity() == opacity &&
      scrim_->GetNativeWindow()->bounds() == bounds) {
    return;
  }

  if (!scrim_)
    scrim_ = CreateScrim(GetTarget(), bounds);
  GetTarget()->parent()->StackChildBelow(scrim_->GetNativeWindow(),
                                         GetTarget());
  scrim_->GetLayer()->SetBackgroundBlur(blur);

  if (scrim_->GetNativeWindow()->bounds() != bounds) {
    scrim_->SetOpacity(0.f);
    scrim_->SetBounds(bounds);
  }
  ui::ScopedLayerAnimationSettings animation(scrim_->GetLayer()->GetAnimator());
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kScrimTransitionInMs));
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  scrim_->SetOpacity(opacity);
}

bool TabletModeWindowResizer::ShouldShowDragIndicators(
    const gfx::Point& location_in_screen) const {
  // Do not show the drag indicators if split view mode is active.
  if (split_view_controller_->IsSplitViewModeActive())
    return false;

  // If the event location hasn't passed the indicator vertical threshold, do
  // not show the drag indicators.
  gfx::Rect work_area_bounds = display::Screen::GetScreen()
                                   ->GetDisplayNearestWindow(GetTarget())
                                   .work_area();
  if (location_in_screen.y() < GetIndicatorsVerticalThreshold(work_area_bounds))
    return false;

  // If the event location has passed the maximize vertical threshold, and the
  // event location is not in snap indicator area, and overview mode is not
  // active at the moment, do not show the drag indicators.
  if (location_in_screen.y() >=
          GetMaximizeVerticalThreshold(work_area_bounds) &&
      GetSnapPosition(location_in_screen) == SplitViewController::NONE &&
      !Shell::Get()->window_selector_controller()->IsSelecting()) {
    return false;
  }

  return true;
}

}  // namespace ash
