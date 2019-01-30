// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_MANUALFILL_KEYBOARD_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_MANUALFILL_KEYBOARD_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/manualfill/manualfill_view_controller.h"

#import "ios/chrome/browser/ui/autofill/manualfill/keyboard_accessory_view.h"

// Subclass of `ManualfillViewController` with the code that is specific for
// devices with no undocked keyboard.
@interface ManualfillKeyboardViewController
    : ManualfillViewController<KeyboardAccessoryViewDelegate>
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_MANUALFILL_KEYBOARD_VIEW_CONTROLLER_H_
