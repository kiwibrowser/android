// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_bubble.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "base/metrics/histogram_macros.h"

namespace ash {

namespace {

constexpr int kPaddingFromScreenTop = 8;

}  // namespace

UnifiedSystemTrayBubble::UnifiedSystemTrayBubble(UnifiedSystemTray* tray,
                                                 bool show_by_click)
    : controller_(std::make_unique<UnifiedSystemTrayController>(tray->model())),
      tray_(tray) {
  if (show_by_click)
    time_shown_by_click_ = base::TimeTicks::Now();

  views::TrayBubbleView::InitParams init_params;
  init_params.anchor_alignment = views::TrayBubbleView::ANCHOR_ALIGNMENT_BOTTOM;
  init_params.min_width = kTrayMenuWidth;
  init_params.max_width = kTrayMenuWidth;
  init_params.delegate = tray;
  init_params.parent_window = tray->GetBubbleWindowContainer();
  init_params.anchor_view =
      tray->shelf()->GetSystemTrayAnchor()->GetBubbleAnchor();
  init_params.corner_radius = kUnifiedTrayCornerRadius;
  init_params.has_shadow = false;
  init_params.show_by_click = show_by_click;
  init_params.close_on_deactivate = false;

  bubble_view_ = new views::TrayBubbleView(init_params);
  int max_height = tray->shelf()->GetUserWorkAreaBounds().height() -
                   kPaddingFromScreenTop -
                   bubble_view_->GetBorderInsets().height();
  unified_view_ = controller_->CreateView();
  time_to_click_recorder_ =
      std::make_unique<TimeToClickRecorder>(this, unified_view_);
  unified_view_->SetMaxHeight(max_height);
  bubble_view_->SetMaxHeight(max_height);
  bubble_view_->AddChildView(unified_view_);
  bubble_view_->set_anchor_view_insets(
      tray->shelf()->GetSystemTrayAnchor()->GetBubbleAnchorInsets());
  bubble_view_->set_color(SK_ColorTRANSPARENT);
  bubble_view_->layer()->SetFillsBoundsOpaquely(false);

  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble_view_);
  bubble_widget_->AddObserver(this);

  TrayBackgroundView::InitializeBubbleAnimations(bubble_widget_);
  bubble_view_->InitializeAndShowBubble();
  if (app_list::features::IsBackgroundBlurEnabled()) {
    // ClientView's layer (See TrayBubbleView::InitializeAndShowBubble())
    bubble_view_->layer()->parent()->SetBackgroundBlur(
        kUnifiedMenuBackgroundBlur);
  }

  tray->tray_event_filter()->AddBubble(this);
}

UnifiedSystemTrayBubble::~UnifiedSystemTrayBubble() {
  tray_->tray_event_filter()->RemoveBubble(this);
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    bubble_widget_->Close();
  }
}

gfx::Rect UnifiedSystemTrayBubble::GetBoundsInScreen() const {
  DCHECK(bubble_view_);
  return bubble_view_->GetBoundsInScreen();
}

bool UnifiedSystemTrayBubble::IsBubbleActive() const {
  return bubble_widget_ && bubble_widget_->IsActive();
}

void UnifiedSystemTrayBubble::ActivateBubble() {
  DCHECK(unified_view_);
  DCHECK(bubble_widget_);
  unified_view_->RequestInitFocus();
  bubble_widget_->widget_delegate()->set_can_activate(true);
  bubble_widget_->Activate();
}

void UnifiedSystemTrayBubble::CloseNow() {
  if (!bubble_widget_)
    return;

  bubble_widget_->RemoveObserver(this);
  bubble_widget_->CloseNow();
  bubble_widget_ = nullptr;
}

TrayBackgroundView* UnifiedSystemTrayBubble::GetTray() const {
  return tray_;
}

views::TrayBubbleView* UnifiedSystemTrayBubble::GetBubbleView() const {
  return bubble_view_;
}

views::Widget* UnifiedSystemTrayBubble::GetBubbleWidget() const {
  return bubble_widget_;
}

void UnifiedSystemTrayBubble::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  tray_->CloseBubble();
}

void UnifiedSystemTrayBubble::RecordTimeToClick() {
  // Ignore if the tray bubble is not opened by click.
  if (!time_shown_by_click_)
    return;

  UMA_HISTOGRAM_TIMES("ChromeOS.SystemTray.TimeToClick",
                      base::TimeTicks::Now() - time_shown_by_click_.value());

  time_shown_by_click_.reset();
}

}  // namespace ash
