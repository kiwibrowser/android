// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_FOCUS_HANDLER_H_
#define ASH_IME_IME_FOCUS_HANDLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/client/focus_change_observer.h"

namespace aura {
namespace client {
class FocusClient;
}  // namespace client
}  // namespace aura

namespace ui {
class InputMethod;
}

namespace ash {

// Updates the focus state of the shared IME instance of ash when the focus
// moves between ash windows and ClientWindows.
class ASH_EXPORT ImeFocusHandler : public aura::client::FocusChangeObserver {
 public:
  ImeFocusHandler(aura::client::FocusClient* focus_client,
                  ui::InputMethod* input_method);
  ~ImeFocusHandler() override;

 private:
  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  aura::client::FocusClient* const focus_client_;

  // IME instance to update. This is the shared IME instance in production.
  ui::InputMethod* const input_method_;

  DISALLOW_COPY_AND_ASSIGN(ImeFocusHandler);
};

}  // namespace ash

#endif  // ASH_IME_IME_FOCUS_HANDLER_H_
