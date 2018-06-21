// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_MAIN_STAGE_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_MAIN_STAGE_H_

#include "base/macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ash {

class AssistantController;
class AssistantQueryView;
class SuggestionContainerView;
class UiElementContainerView;

// AssistantMainStage is the child of AssistantMainView responsible for
// displaying the Assistant interaction to the user. This includes visual
// affordances for the query, response, as well as suggestions.
class AssistantMainStage : public views::View, public views::ViewObserver {
 public:
  explicit AssistantMainStage(AssistantController* assistant_controller);
  ~AssistantMainStage() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override;
  void OnViewVisibilityChanged(views::View* view) override;

 private:
  void InitLayout(AssistantController* assistant_controller);
  void InitContentLayoutContainer(AssistantController* assistant_controller);
  void InitQueryLayoutContainer(AssistantController* assistant_controller);

  AssistantQueryView* committed_query_view_;       // Owned by view hierarchy.
  views::View* committed_query_view_spacer_;       // Owned by view hierarchy.
  AssistantQueryView* pending_query_view_;         // Owned by view hierarchy.
  SuggestionContainerView* suggestion_container_;  // Owned by view hierarchy.
  views::View* suggestion_container_spacer_;       // Owned by view hierarchy.
  UiElementContainerView* ui_element_container_;   // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantMainStage);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_MAIN_STAGE_H_
