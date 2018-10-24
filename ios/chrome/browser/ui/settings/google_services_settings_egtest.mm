// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsDoneButton;

// Integration tests using the Google services settings screen.
@interface GoogleServicesSettingsTestCase : ChromeTestCase
@end

@implementation GoogleServicesSettingsTestCase

// Opens the Google services settings view, and closes it.
- (void)testOpenGoogleServicesSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];

  // Assert title and accessibility.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"google_services_settings_view_controller")]
      assertWithMatcher:grey_notNil()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
