// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/input_events_blocker.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event.h"

namespace chromeos {

InputEventsBlocker::InputEventsBlocker() {
  // TODO(mash): Implement a mash version. This will probably need to talk to
  // the window server.
  if (features::IsAshInBrowserProcess()) {
    ash::Shell::Get()->AddPreTargetHandler(this,
                                           ui::EventTarget::Priority::kSystem);
    VLOG(1) << "InputEventsBlocker " << this << " created.";
  } else {
    NOTIMPLEMENTED();
  }
}

InputEventsBlocker::~InputEventsBlocker() {
  if (features::IsAshInBrowserProcess()) {
    ash::Shell::Get()->RemovePreTargetHandler(this);
    VLOG(1) << "InputEventsBlocker " << this << " destroyed.";
  } else {
    NOTIMPLEMENTED();
  }
}

void InputEventsBlocker::OnKeyEvent(ui::KeyEvent* event) {
  event->StopPropagation();
}

void InputEventsBlocker::OnMouseEvent(ui::MouseEvent* event) {
  event->StopPropagation();
}

void InputEventsBlocker::OnTouchEvent(ui::TouchEvent* event) {
  event->StopPropagation();
}

void InputEventsBlocker::OnGestureEvent(ui::GestureEvent* event) {
  event->StopPropagation();
}

}  // namespace chromeos
