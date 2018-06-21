// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_main_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/assistant_main_stage.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMinHeightDip = 200;

}  // namespace

AssistantMainView::AssistantMainView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      min_height_dip_(kMinHeightDip) {
  InitLayout();

  // Set delegates.
  caption_bar_->set_delegate(assistant_controller_->ui_controller());
  dialog_plate_->set_delegate(assistant_controller_);

  // Observe changes to interaction model.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

AssistantMainView::~AssistantMainView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size AssistantMainView::CalculatePreferredSize() const {
  // |min_height_dip_| <= |preferred_height| <= |kMaxHeightDip|.
  int preferred_height = GetHeightForWidth(kPreferredWidthDip);
  preferred_height = std::min(preferred_height, kMaxHeightDip);
  preferred_height = std::max(preferred_height, min_height_dip_);
  return gfx::Size(kPreferredWidthDip, preferred_height);
}

void AssistantMainView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  // Until Assistant UI is hidden, the view may grow in height but not shrink.
  min_height_dip_ = std::max(min_height_dip_, height());
}

void AssistantMainView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // Even though the preferred size for |main_stage_| may change, its bounds
  // may not actually change due to height restrictions imposed by its parent.
  // For this reason, we need to explicitly trigger a layout pass so that the
  // children of |main_stage_| are properly updated.
  if (child == main_stage_) {
    Layout();
    SchedulePaint();
  }
}

void AssistantMainView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Caption bar.
  caption_bar_ = new CaptionBar();
  AddChildView(caption_bar_);

  // Main stage.
  main_stage_ = new AssistantMainStage(assistant_controller_);
  AddChildView(main_stage_);

  layout_manager->SetFlexForView(main_stage_, 1);

  // Dialog plate.
  dialog_plate_ = new DialogPlate(assistant_controller_);
  AddChildView(dialog_plate_);
}

void AssistantMainView::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kInactive)
    return;

  // When the Assistant UI is being hidden we need to reset our minimum height
  // restriction so that the default restrictions are restored for the next
  // time the view is shown.
  min_height_dip_ = kMinHeightDip;
  PreferredSizeChanged();
}

void AssistantMainView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

}  // namespace ash
