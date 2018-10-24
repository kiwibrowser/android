// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_STEADY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_STEADY_VIEW_H_

#import <UIKit/UIKit.h>

// A color scheme used for the steady view elements.
@interface LocationBarSteadyViewColorScheme : NSObject

// The color of the location label and the location icon.
@property(nonatomic, strong) UIColor* fontColor;
// The color of the placeholder string.
@property(nonatomic, strong) UIColor* placeholderColor;
// The tint color of the trailing button.
@property(nonatomic, strong) UIColor* trailingButtonColor;

+ (LocationBarSteadyViewColorScheme*)standardScheme;
+ (LocationBarSteadyViewColorScheme*)incognitoScheme;

@end

// A simple view displaying the current URL and security status icon.
@interface LocationBarSteadyView : UIButton

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (void)setColorScheme:(LocationBarSteadyViewColorScheme*)colorScheme;

// The label displaying the current location URL.
@property(nonatomic, strong) UILabel* locationLabel;
// The image view displaying the current location icon (i.e. http[s] status).
@property(nonatomic, strong) UIImageView* locationIconImageView;
// The button displayed in the trailing corner of the view, i.e. share button.
@property(nonatomic, strong) UIButton* trailingButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_STEADY_VIEW_H_
