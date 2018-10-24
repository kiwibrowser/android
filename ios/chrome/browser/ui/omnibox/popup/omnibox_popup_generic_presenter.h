// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_GENERIC_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_GENERIC_PRESENTER_H_

#import <UIKit/UIKit.h>

@protocol OmniboxPopupPositioner;

// A generic omnibox popup presenter, serving as a common API for the UI Refresh
// and Legacy implementations.
@protocol OmniboxPopupGenericPresenter<NSObject>

// Updates appearance depending on the content size of the presented view
// controller by changing the visible height of the popup. When the popup was
// not previously shown, it will appear with "expansion" animation.
- (void)updateHeightAndAnimateAppearanceIfNecessary;
// Call this to hide the popup with animation.
- (void)animateCollapse;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_OMNIBOX_POPUP_GENERIC_PRESENTER_H_
