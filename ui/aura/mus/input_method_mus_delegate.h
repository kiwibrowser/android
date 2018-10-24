// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_INPUT_METHOD_MUS_DELEGATE_H_
#define UI_AURA_MUS_INPUT_METHOD_MUS_DELEGATE_H_

#include "services/ui/public/interfaces/ime/ime.mojom.h"
#include "ui/aura/aura_export.h"

namespace aura {

// Used by InputMethodMus to update the ime related state of the associated
// window.
class AURA_EXPORT InputMethodMusDelegate {
 public:
  virtual void SetTextInputState(ui::mojom::TextInputStatePtr state) = 0;
  virtual void SetImeVisibility(bool visible,
                                ui::mojom::TextInputStatePtr state) = 0;

 protected:
  virtual ~InputMethodMusDelegate() {}
};

}  // namespace aura

#endif  // UI_AURA_MUS_INPUT_METHOD_MUS_DELEGATE_H_
