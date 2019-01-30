// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMinHeightDip = 32;

}  // namespace

AssistantQueryView::AssistantQueryView(
    AssistantController* assistant_controller,
    ObservedQueryState observed_query_state)
    : assistant_controller_(assistant_controller),
      observed_query_state_(observed_query_state) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // AssistantQueryView belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

AssistantQueryView::~AssistantQueryView() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

gfx::Size AssistantQueryView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantQueryView::GetHeightForWidth(int width) const {
  return std::max(views::View::GetHeightForWidth(width), kMinHeightDip);
}

void AssistantQueryView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantQueryView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  label_->SizeToFit(width());
}

void AssistantQueryView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::MAIN_AXIS_ALIGNMENT_CENTER);

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Label.
  label_ = new views::StyledLabel(base::string16(), /*listener=*/nullptr);
  label_->set_auto_color_readability_enabled(false);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  AddChildView(label_);

  // Artificially trigger event to initialize state.
  OnPendingQueryChanged(observed_query_state_ == ObservedQueryState::kCommitted
                            ? assistant_controller_->interaction_controller()
                                  ->model()
                                  ->committed_query()
                            : assistant_controller_->interaction_controller()
                                  ->model()
                                  ->pending_query());
}

void AssistantQueryView::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  if (observed_query_state_ != ObservedQueryState::kCommitted)
    return;

  OnQueryChanged(committed_query);
}

void AssistantQueryView::OnPendingQueryChanged(
    const AssistantQuery& pending_query) {
  if (observed_query_state_ != ObservedQueryState::kPending)
    return;

  OnQueryChanged(pending_query);
}

void AssistantQueryView::OnQueryChanged(const AssistantQuery& query) {
  // Empty query.
  if (query.Empty()) {
    OnQueryCleared();
  } else {
    // Populated query.
    switch (query.type()) {
      case AssistantQueryType::kText: {
        const AssistantTextQuery& text_query =
            static_cast<const AssistantTextQuery&>(query);
        SetText(text_query.text());
        break;
      }
      case AssistantQueryType::kVoice: {
        const AssistantVoiceQuery& voice_query =
            static_cast<const AssistantVoiceQuery&>(query);
        SetText(voice_query.high_confidence_speech(),
                voice_query.low_confidence_speech());
        break;
      }
      case AssistantQueryType::kEmpty:
        // Empty queries are already handled.
        NOTREACHED();
        break;
    }
  }
}

void AssistantQueryView::OnCommittedQueryCleared() {
  if (observed_query_state_ != ObservedQueryState::kCommitted)
    return;

  OnQueryCleared();
}

void AssistantQueryView::OnPendingQueryCleared() {
  if (observed_query_state_ != ObservedQueryState::kPending)
    return;

  OnQueryCleared();
}

void AssistantQueryView::OnQueryCleared() {
  SetVisible(false);
  label_->SetText(base::string16());
}

void AssistantQueryView::SetText(const std::string& high_confidence_text,
                                 const std::string& low_confidence_text) {
  if (observed_query_state_ == ObservedQueryState::kCommitted) {
    // When observing a committed query, text is displayed in a single color.
    const base::string16& text_16 =
        base::UTF8ToUTF16(high_confidence_text + low_confidence_text);

    label_->SetText(text_16);
    label_->AddStyleRange(gfx::Range(0, text_16.length()),
                          CreateStyleInfo(kTextColorSecondary));
  } else {
    // When observing a pending query, high confidence text and low confidence
    // text are displayed in different colors for visual emphasis.
    const base::string16& high_confidence_text_16 =
        base::UTF8ToUTF16(high_confidence_text);

    if (low_confidence_text.empty()) {
      label_->SetText(high_confidence_text_16);
      label_->AddStyleRange(gfx::Range(0, high_confidence_text_16.length()),
                            CreateStyleInfo(kTextColorPrimary));
    } else {
      const base::string16& low_confidence_text_16 =
          base::UTF8ToUTF16(low_confidence_text);

      label_->SetText(high_confidence_text_16 + low_confidence_text_16);

      // High confidence text styling.
      label_->AddStyleRange(gfx::Range(0, high_confidence_text_16.length()),
                            CreateStyleInfo(kTextColorPrimary));

      // Low confidence text styling.
      label_->AddStyleRange(gfx::Range(high_confidence_text_16.length(),
                                       high_confidence_text_16.length() +
                                           low_confidence_text_16.length()),
                            CreateStyleInfo(kTextColorHint));
    }
  }

  label_->SizeToFit(width());
  PreferredSizeChanged();
  SetVisible(true);
}

views::StyledLabel::RangeStyleInfo AssistantQueryView::CreateStyleInfo(
    SkColor color) const {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = label_->GetDefaultFontList().DeriveWithSizeDelta(2);
  style.override_color = color;
  return style;
}

}  // namespace ash
