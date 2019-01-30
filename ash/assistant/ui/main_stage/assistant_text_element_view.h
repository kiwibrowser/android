// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/label.h"

namespace ash {

class AssistantTextElement;

// AssistantTextElementView is the visual representation of an
// AssistantTextElement. It is a child view of UiElementContainerView.
class AssistantTextElementView : public views::Label {
 public:
  explicit AssistantTextElementView(const AssistantTextElement* text_element);
  ~AssistantTextElementView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantTextElementView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_TEXT_ELEMENT_VIEW_H_
