// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
#define ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AssistantController;

class AssistantMiniView : public views::View,
                          public AssistantInteractionModelObserver {
 public:
  explicit AssistantMiniView(AssistantController* assistant_controller);
  ~AssistantMiniView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnInputModalityChanged(InputModality input_modality) override;

 private:
  void InitLayout();

  AssistantController* const assistant_controller_;  // Owned by Shell.
  views::Label* label_;                              // Owned by view hierarchy.

  DISALLOW_COPY_AND_ASSIGN(AssistantMiniView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_MINI_VIEW_H_
