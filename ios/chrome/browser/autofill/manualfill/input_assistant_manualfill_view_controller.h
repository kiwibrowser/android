// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_INPUT_ASSISTANT_MANUALFILL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_INPUT_ASSISTANT_MANUALFILL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/autofill/manualfill/manualfill_view_controller.h"

// This class allows the user to manual fill data by adding input assistant
// items  to the first responder. Which then uses pop overs to show the
// available options. Currently the `inputAssistantItem` property is only
// available on iPads.
@interface InputAssistantManualfillViewController : ManualfillViewController
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_INPUT_ASSISTANT_MANUALFILL_VIEW_CONTROLLER_H_
