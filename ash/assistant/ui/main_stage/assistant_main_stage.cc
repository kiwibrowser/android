// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_main_stage.h"

#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/ui/main_stage/assistant_query_view.h"
#include "ash/assistant/ui/main_stage/suggestion_container_view.h"
#include "ash/assistant/ui/main_stage/ui_element_container_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

AssistantMainStage::AssistantMainStage(
    AssistantController* assistant_controller) {
  InitLayout(assistant_controller);
}

AssistantMainStage::~AssistantMainStage() = default;

void AssistantMainStage::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewPreferredSizeChanged(views::View* view) {
  if (view == committed_query_view_) {
    // Because it reserves layout space for |committed_query_view_|, the
    // associated spacer needs to match its preferred size.
    committed_query_view_spacer_->SetPreferredSize(
        committed_query_view_->GetPreferredSize());
  } else if (view == suggestion_container_) {
    // Because it reserves layout space for the |suggestion_container_|, the
    // associated spacer needs to match its preferred size.
    suggestion_container_spacer_->SetPreferredSize(
        suggestion_container_->GetPreferredSize());
  }
  PreferredSizeChanged();
}

void AssistantMainStage::OnViewVisibilityChanged(views::View* view) {
  if (view == committed_query_view_) {
    // We only reserve space for |committed_query_view_| when it is visible.
    committed_query_view_spacer_->SetVisible(committed_query_view_->visible());
  } else if (view == suggestion_container_) {
    // We only reserve space for the |suggestion_container_| when it is hidden.
    suggestion_container_spacer_->SetVisible(!suggestion_container_->visible());
  } else if (view == pending_query_view_) {
    // We only display |suggestion_container_| when |pending_query_view_| is
    // hidden. When |suggestion_container_| is hidden, its space will be
    // preserved in the layout by |suggestion_container_spacer_|.
    suggestion_container_->SetVisible(!pending_query_view_->visible());
  }
  PreferredSizeChanged();
}

void AssistantMainStage::InitLayout(AssistantController* assistant_controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  InitContentLayoutContainer(assistant_controller);
  InitQueryLayoutContainer(assistant_controller);
}

void AssistantMainStage::InitContentLayoutContainer(
    AssistantController* assistant_controller) {
  // Note that we will observe children of |content_layout_container| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |content_layout_container| may not change size in
  // response to these events, necessitating an explicit layout pass.
  views::View* content_layout_container = new views::View();

  views::BoxLayout* layout_manager = content_layout_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Committed query spacer.
  // Note: This view reserves layout space for |committed_query_view_|,
  // dynamically mirroring its preferred size and visibility.
  committed_query_view_spacer_ = new views::View();
  committed_query_view_spacer_->AddObserver(this);
  content_layout_container->AddChildView(committed_query_view_spacer_);

  // UI element container.
  ui_element_container_ = new UiElementContainerView(assistant_controller);
  ui_element_container_->AddObserver(this);
  content_layout_container->AddChildView(ui_element_container_);

  layout_manager->SetFlexForView(ui_element_container_, 1);

  // Suggestion container.
  suggestion_container_ = new SuggestionContainerView(assistant_controller);
  suggestion_container_->AddObserver(this);
  content_layout_container->AddChildView(suggestion_container_);

  // Suggestion container spacer.
  // Note: This view reserves layout space for the |suggestion_container_|,
  // dynamically mirroring its preferred size and being visible only when the
  // |suggestion_container_| is hidden.
  suggestion_container_spacer_ = new views::View();
  content_layout_container->AddChildView(suggestion_container_spacer_);

  AddChildView(content_layout_container);
}

void AssistantMainStage::InitQueryLayoutContainer(
    AssistantController* assistant_controller) {
  // Note that we will observe children of |query_layout_container| to handle
  // preferred size and visibility change events in AssistantMainStage. This is
  // necessary because |query_layout_container| may not change size in response
  // to these events, necessitating an explicit layout pass.
  views::View* query_layout_container = new views::View();
  query_layout_container->set_can_process_events_within_subtree(false);

  views::BoxLayout* layout_manager = query_layout_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Committed query.
  committed_query_view_ = new AssistantQueryView(
      assistant_controller, AssistantQueryView::ObservedQueryState::kCommitted);
  committed_query_view_->AddObserver(this);
  query_layout_container->AddChildView(committed_query_view_);

  // Spacer.
  views::View* spacer = new views::View();
  query_layout_container->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Pending query.
  pending_query_view_ = new AssistantQueryView(
      assistant_controller, AssistantQueryView::ObservedQueryState::kPending);
  pending_query_view_->AddObserver(this);
  query_layout_container->AddChildView(pending_query_view_);

  AddChildView(query_layout_container);
}

}  // namespace ash
