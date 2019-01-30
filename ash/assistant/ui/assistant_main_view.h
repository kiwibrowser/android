// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantController;
class AssistantMainStage;
class CaptionBar;
class DialogPlate;

class AssistantMainView : public views::View,
                          public AssistantInteractionModelObserver {
 public:
  explicit AssistantMainView(AssistantController* assistant_controller);
  ~AssistantMainView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  void OnBoundsChanged(const gfx::Rect& prev_bounds) override;
  void RequestFocus() override;

  // AssistantInteractionModelObserver:
  void OnInteractionStateChanged(InteractionState interaction_state) override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  CaptionBar* caption_bar_;                         // Owned by view hierarchy.
  DialogPlate* dialog_plate_;                       // Owned by view hierarchy.
  AssistantMainStage* main_stage_;                  // Owned by view hierarchy.

  int min_height_dip_;

  DISALLOW_COPY_AND_ASSIGN(AssistantMainView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MAIN_VIEW_H_
