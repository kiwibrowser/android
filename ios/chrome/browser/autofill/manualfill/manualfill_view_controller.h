// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_MANUALFILL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_MANUALFILL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

namespace manualfill {

// Searches for the first responder in the passed view hierarchy.
//
// @param view The view where the search is going to be done.
// @return The first responder or `nil` if it wasn't found.
UIView* GetFirstResponderSubview(UIView* view);

}  // namespace manualfill

// Protocol to pass any user choice in a picker to be filled.
@protocol ManualFillContentDelegate<NSObject>

// Called after the user manually selects an element to be used as the input.
//
// @param content The string that is interesting to the user in the current
//                context.
- (void)userDidPickContent:(NSString*)content;

@end

// View Controller with the common logic for managing the manual fill views. As
// well as sending user input to the web view. Meant to be subclassed.
@interface ManualfillViewController
    : UIViewController<ManualFillContentDelegate>

// The web view to test the prototype.
@property(nonatomic, readonly) WKWebView* webView;

// The last known first responder.
@property(nonatomic) UIView* lastFirstResponder;

// Asynchronously updates the activeFieldID to the current active element.
// Must be called before the web view resigns first responder.
- (void)updateActiveFieldID;

// Tries to inject the passed string into the web view last focused field.
//
// @param string The content to be injected. Must be JS encoded.
- (void)fillLastSelectedFieldWithString:(NSString*)string;

// Calls JS `focus()` on the last active element in an attemp to highlight it.
- (void)callFocusOnLastActiveField;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_MANUALFILL_MANUALFILL_VIEW_CONTROLLER_H_
