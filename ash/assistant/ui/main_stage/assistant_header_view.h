// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
}  // namespace views

namespace ash {

class AssistantController;

// AssistantHeaderView is the child of UiElementContainerView which provides
// the Assistant icon. On first launch, it also displays a greeting to the user.
class AssistantHeaderView : public views::View,
                            public AssistantInteractionModelObserver {
 public:
  explicit AssistantHeaderView(AssistantController* assistant_controller);
  ~AssistantHeaderView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildVisibilityChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;
  void OnCommittedQueryChanged(const AssistantQuery& committed_query) override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  views::BoxLayout* layout_manager_;  // Owned by view hierarchy.
  views::Label* label_;               // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantHeaderView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_HEADER_VIEW_H_
