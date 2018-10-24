// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_mini_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 20;
constexpr int kMaxWidthDip = 512;
constexpr int kPreferredHeightDip = 48;

}  // namespace

AssistantMiniView::AssistantMiniView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), label_(new views::Label()) {
  InitLayout();

  // AssistantController indirectly owns the view hierarchy to which
  // AssistantMiniView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

AssistantMiniView::~AssistantMiniView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size AssistantMiniView::CalculatePreferredSize() const {
  const int preferred_width =
      std::min(views::View::CalculatePreferredSize().width(), kMaxWidthDip);
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int AssistantMiniView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantMiniView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMiniView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip), 2 * kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Icon.
  views::ImageView* icon = new views::ImageView();
  icon->SetImage(gfx::CreateVectorIcon(kAssistantIcon, kIconSizeDip));
  icon->SetImageSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon);

  // Label.
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(4));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  AddChildView(label_);

  // Trigger input modality changed event to initialize view state.
  OnInputModalityChanged(assistant_controller_->interaction_controller()
                             ->model()
                             ->input_modality());
}

void AssistantMiniView::OnInputModalityChanged(InputModality input_modality) {
  switch (input_modality) {
    case InputModality::kStylus:
      label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_STYLUS));
      break;
    case InputModality::kKeyboard:
    case InputModality::kVoice:
      label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
      break;
  }
}

}  // namespace ash
