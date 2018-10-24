// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}

@protocol BubblePresenterDelegate;
@class BubbleViewControllerPresenter;
@protocol ToolbarCommands;

// Object handling the presentation of the different bubbles tips. The class is
// holding all the bubble presenters.
@interface BubblePresenter : NSObject

// Initializes a BubblePresenter whose bubbles are presented on the
// |rootViewController|.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<BubblePresenterDelegate>)delegate
                  rootViewController:(UIViewController*)rootViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Used to display the new incognito tab tip in-product help promotion bubble.
@property(nonatomic, strong, readonly)
    BubbleViewControllerPresenter* incognitoTabTipBubblePresenter;

@property(nonatomic, weak) id<ToolbarCommands> dispatcher;

// Presents either in-product help bubbles if the the user is in a valid state
// to see one of them. At most one bubble will be shown. If the feature
// engagement tracker determines it is not valid to see one of the bubbles, that
// bubble will not be shown.
- (void)presentBubblesIfEligible;

// Dismisses all bubbles.
- (void)dismissBubbles;

// Notifies the presenter that the user entered the tab switcher.
- (void)userEnteredTabSwitcher;

// Notifies the presenter that the tools menu has been displayed.
- (void)toolsMenuDisplayed;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_PRESENTER_H_
