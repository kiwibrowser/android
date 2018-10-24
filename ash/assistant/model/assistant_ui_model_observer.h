// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_

#include "base/macros.h"

namespace ash {

enum class AssistantUiMode;

// An observer which receives notification of changes to the Assistant UI model.
class AssistantUiModelObserver {
 public:
  // Invoked when the UI mode is changed.
  virtual void OnUiModeChanged(AssistantUiMode ui_mode) {}

 protected:
  AssistantUiModelObserver() = default;
  virtual ~AssistantUiModelObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantUiModelObserver);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_UI_MODEL_OBSERVER_H_
