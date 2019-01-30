// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "ash/public/cpp/ash_features.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace keyboard_shortcut_viewer_util {
namespace {

// Keyboard shortcut viewer app is incompatible with some a11y features.
bool IsUsingA11yIncompatibleWithApp() {
  chromeos::AccessibilityManager* a11y = chromeos::AccessibilityManager::Get();
  return a11y->IsSpokenFeedbackEnabled() || a11y->IsCaretHighlightEnabled() ||
         a11y->IsFocusHighlightEnabled() || a11y->IsSelectToSpeakEnabled() ||
         a11y->IsSwitchAccessEnabled();
}

}  // namespace

void ShowKeyboardShortcutViewer() {
  base::TimeTicks user_gesture_time = base::TimeTicks::Now();
  if (ash::features::IsKeyboardShortcutViewerAppEnabled() &&
      !IsUsingA11yIncompatibleWithApp()) {
    shortcut_viewer::mojom::ShortcutViewerPtr shortcut_viewer_ptr;
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(shortcut_viewer::mojom::kServiceName,
                             &shortcut_viewer_ptr);
    shortcut_viewer_ptr->Toggle(user_gesture_time);
  } else {
    keyboard_shortcut_viewer::KeyboardShortcutView::Toggle(user_gesture_time);
  }
}

}  // namespace keyboard_shortcut_viewer_util
