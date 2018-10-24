// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_test_api.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

UnifiedSystemTrayTestApi::UnifiedSystemTrayTestApi(UnifiedSystemTray* tray)
    : tray_(tray) {}

UnifiedSystemTrayTestApi::~UnifiedSystemTrayTestApi() = default;

// static
void UnifiedSystemTrayTestApi::BindRequest(
    mojom::SystemTrayTestApiRequest request) {
  UnifiedSystemTray* tray = Shell::Get()
                                ->GetPrimaryRootWindowController()
                                ->GetStatusAreaWidget()
                                ->unified_system_tray();
  mojo::MakeStrongBinding(std::make_unique<UnifiedSystemTrayTestApi>(tray),
                          std::move(request));
}

void UnifiedSystemTrayTestApi::DisableAnimations(DisableAnimationsCallback cb) {
  disable_animations_ = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  std::move(cb).Run();
}

void UnifiedSystemTrayTestApi::IsTrayBubbleOpen(IsTrayBubbleOpenCallback cb) {
  std::move(cb).Run(tray_->IsBubbleShown());
}

void UnifiedSystemTrayTestApi::IsTrayViewVisible(int view_id,
                                                 IsTrayViewVisibleCallback cb) {
  std::move(cb).Run(false);
}

void UnifiedSystemTrayTestApi::ShowBubble(ShowBubbleCallback cb) {
  tray_->ShowBubble(false /* show_by_click */);
  std::move(cb).Run();
}

void UnifiedSystemTrayTestApi::CloseBubble(CloseBubbleCallback cb) {
  tray_->CloseBubble();
  std::move(cb).Run();
}

void UnifiedSystemTrayTestApi::ShowDetailedView(mojom::TrayItem item,
                                                ShowDetailedViewCallback cb) {
  switch (item) {
    case mojom::TrayItem::kEnterprise:
      // TODO(tetsui): Remove this from .mojom, because both SystemTray and
      // UnifiedSystemTray do not have enterprise detailed view, and this is not
      // used from anywhere.
      NOTREACHED();
      break;
    case mojom::TrayItem::kNetwork:
      tray_->ShowBubble(false /* show_by_click */);
      tray_->bubble_->controller_->ShowNetworkDetailedView();
      break;
  }
  std::move(cb).Run();
}

void UnifiedSystemTrayTestApi::IsBubbleViewVisible(
    int view_id,
    IsBubbleViewVisibleCallback cb) {
  views::View* view = GetBubbleView(view_id);
  std::move(cb).Run(view && view->visible());
}

void UnifiedSystemTrayTestApi::GetBubbleViewTooltip(
    int view_id,
    GetBubbleViewTooltipCallback cb) {
  base::string16 tooltip;
  views::View* view = GetBubbleView(view_id);
  if (view)
    view->GetTooltipText(gfx::Point(), &tooltip);
  std::move(cb).Run(tooltip);
}

void UnifiedSystemTrayTestApi::GetBubbleLabelText(
    int view_id,
    GetBubbleLabelTextCallback cb) {
  base::string16 text;
  views::View* view = GetBubbleView(view_id);
  if (view)
    text = static_cast<views::Label*>(view)->text();
  std::move(cb).Run(text);
}

void UnifiedSystemTrayTestApi::Is24HourClock(Is24HourClockCallback cb) {
  base::HourClockType type = tray_->time_view_->GetHourTypeForTesting();
  std::move(cb).Run(type == base::k24HourClock);
}

views::View* UnifiedSystemTrayTestApi::GetBubbleView(int view_id) const {
  return tray_->bubble_->bubble_view_->GetViewByID(view_id);
}

}  // namespace ash
