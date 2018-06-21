// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_focus_handler.h"

#include "base/logging.h"
#include "services/ui/ws2/window_service.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/ime/input_method.h"

namespace ash {

ImeFocusHandler::ImeFocusHandler(aura::client::FocusClient* focus_client,
                                 ui::InputMethod* input_method)
    : focus_client_(focus_client), input_method_(input_method) {
  DCHECK(input_method_);

  focus_client_->AddObserver(this);
}

ImeFocusHandler::~ImeFocusHandler() {
  focus_client_->RemoveObserver(this);
}

void ImeFocusHandler::OnWindowFocused(aura::Window* gained_focus,
                                      aura::Window* lost_focus) {
  const bool client_window_gaining_focus =
      ui::ws2::WindowService::HasRemoteClient(gained_focus);
  const bool client_window_losing_focus =
      ui::ws2::WindowService::HasRemoteClient(lost_focus);

  // Focus moves to a ClientWindow from an ash window.
  if (client_window_gaining_focus && !client_window_losing_focus)
    input_method_->OnBlur();

  // Focus moves to an ash window from a ClientWindow.
  if (!client_window_gaining_focus && client_window_losing_focus)
    input_method_->OnFocus();
}

}  // namespace ash
