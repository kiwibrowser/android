// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_header_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kInitialHeightDip = 72;

}  // namespace

AssistantHeaderView::AssistantHeaderView(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // AssistantHeaderView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

AssistantHeaderView::~AssistantHeaderView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size AssistantHeaderView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantHeaderView::GetHeightForWidth(int width) const {
  return label_->visible() ? kInitialHeightDip
                           : views::View::GetHeightForWidth(width);
}

void AssistantHeaderView::ChildVisibilityChanged(views::View* child) {
  DCHECK_EQ(child, label_);

  layout_manager_->set_cross_axis_alignment(
      label_->visible()
          ? views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER
          : views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_START);

  PreferredSizeChanged();
}

void AssistantHeaderView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), kSpacingDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Icon.
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
  icon->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon);

  // Label.
  label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(kTextColorPrimary);
  label_->SetFontList(views::Label::GetDefaultFontList()
                          .DeriveWithSizeDelta(8)
                          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label_->SetMultiLine(true);
  AddChildView(label_);
}

void AssistantHeaderView::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state != InteractionState::kInactive)
    return;

  label_->SetVisible(true);
}

void AssistantHeaderView::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  label_->SetVisible(false);
}

}  // namespace ash
