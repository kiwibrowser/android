// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUALFILL_KEYBOARD_BACKGROUND_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUALFILL_KEYBOARD_BACKGROUND_VIEW_H_

#import <UIKit/UIKit.h>

@protocol KeyboardAccessoryViewDelegate;

// View to show behind the keyboard. It contains an Accessory View and a
// Container for more detailed content.
@interface KeyboardBackgroundView : UIView

// View to contain a picker (addresses, credit cards or credentials) for the
// user.
@property(nonatomic, strong) UIView* containerView;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)init NS_UNAVAILABLE;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

// Unavailable. Use `initWithDelegate:`.
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Instances an object with the desired delegate.
//
// @param delegate The delegate for this object.
// @return A fresh object with the passed delegate.
- (instancetype)initWithDelegate:(id<KeyboardAccessoryViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUALFILL_KEYBOARD_BACKGROUND_VIEW_H_
