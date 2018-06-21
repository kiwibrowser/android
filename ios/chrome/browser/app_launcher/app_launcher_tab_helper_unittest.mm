// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper.h"

#include <memory>

#include "base/compiler_specific.h"
#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/web/app_launcher_abuse_detector.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// An object that conforms to AppLauncherTabHelperDelegate for testing.
@interface FakeAppLauncherTabHelperDelegate
    : NSObject<AppLauncherTabHelperDelegate>
// URL of the last launched application.
@property(nonatomic, assign) GURL lastLaunchedAppURL;
// Number of times an app was launched.
@property(nonatomic, assign) NSUInteger countOfAppsLaunched;
// Number of times the repeated launches alert has been shown.
@property(nonatomic, assign) NSUInteger countOfAlertsShown;
// Simulates the user tapping the accept button when prompted via
// |-appLauncherTabHelper:showAlertOfRepeatedLaunchesWithCompletionHandler|.
@property(nonatomic, assign) BOOL simulateUserAcceptingPrompt;
@end

@implementation FakeAppLauncherTabHelperDelegate
@synthesize lastLaunchedAppURL = _lastLaunchedAppURL;
@synthesize countOfAppsLaunched = _countOfAppsLaunched;
@synthesize countOfAlertsShown = _countOfAlertsShown;
@synthesize simulateUserAcceptingPrompt = _simulateUserAcceptingPrompt;
- (BOOL)appLauncherTabHelper:(AppLauncherTabHelper*)tabHelper
            launchAppWithURL:(const GURL&)URL
                  linkTapped:(BOOL)linkTapped {
  self.countOfAppsLaunched++;
  self.lastLaunchedAppURL = URL;
  return YES;
}
- (void)appLauncherTabHelper:(AppLauncherTabHelper*)tabHelper
    showAlertOfRepeatedLaunchesWithCompletionHandler:
        (ProceduralBlockWithBool)completionHandler {
  self.countOfAlertsShown++;
  completionHandler(self.simulateUserAcceptingPrompt);
}
@end

// An AppLauncherAbuseDetector for testing.
@interface FakeAppLauncherAbuseDetector : AppLauncherAbuseDetector
@property(nonatomic, assign) ExternalAppLaunchPolicy policy;
@end

@implementation FakeAppLauncherAbuseDetector
@synthesize policy = _policy;
- (ExternalAppLaunchPolicy)launchPolicyForURL:(const GURL&)URL
                            fromSourcePageURL:(const GURL&)sourcePageURL {
  return self.policy;
}
@end

// Test fixture for AppLauncherTabHelper class.
class AppLauncherTabHelperTest : public PlatformTest {
 protected:
  AppLauncherTabHelperTest()
      : abuse_detector_([[FakeAppLauncherAbuseDetector alloc] init]),
        delegate_([[FakeAppLauncherTabHelperDelegate alloc] init]) {
    AppLauncherTabHelper::CreateForWebState(&web_state_, abuse_detector_,
                                            delegate_);
    tab_helper_ = AppLauncherTabHelper::FromWebState(&web_state_);
  }

  bool VerifyRequestAllowed(NSString* url_string,
                            bool target_frame_is_main,
                            bool has_user_gesture) WARN_UNUSED_RESULT {
    NSURL* url = [NSURL URLWithString:url_string];
    web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_LINK, target_frame_is_main,
        has_user_gesture);
    return tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                           request_info);
  }

  web::TestWebState web_state_;
  FakeAppLauncherAbuseDetector* abuse_detector_ = nil;
  FakeAppLauncherTabHelperDelegate* delegate_ = nil;
  AppLauncherTabHelper* tab_helper_;
};

// Tests that an empty URL does not show alert or launch app.
TEST_F(AppLauncherTabHelperTest, EmptyUrl) {
  tab_helper_->RequestToLaunchApp(GURL::EmptyGURL(), GURL::EmptyGURL(), false);
  EXPECT_EQ(0U, delegate_.countOfAlertsShown);
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that an invalid URL does not show alert or launch app.
TEST_F(AppLauncherTabHelperTest, InvalidUrl) {
  tab_helper_->RequestToLaunchApp(GURL("invalid"), GURL::EmptyGURL(), false);
  EXPECT_EQ(0U, delegate_.countOfAlertsShown);
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that a valid URL does launch app.
TEST_F(AppLauncherTabHelperTest, ValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
  tab_helper_->RequestToLaunchApp(GURL("valid://1234"), GURL::EmptyGURL(),
                                  false);
  EXPECT_EQ(1U, delegate_.countOfAppsLaunched);
  EXPECT_EQ(GURL("valid://1234"), delegate_.lastLaunchedAppURL);
}

// Tests that a valid URL does not launch app when launch policy is to block.
TEST_F(AppLauncherTabHelperTest, ValidUrlBlocked) {
  abuse_detector_.policy = ExternalAppLaunchPolicyBlock;
  tab_helper_->RequestToLaunchApp(GURL("valid://1234"), GURL::EmptyGURL(),
                                  false);
  EXPECT_EQ(0U, delegate_.countOfAlertsShown);
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that a valid URL shows an alert and launches app when launch policy is
// to prompt and user accepts.
TEST_F(AppLauncherTabHelperTest, ValidUrlPromptUserAccepts) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  delegate_.simulateUserAcceptingPrompt = YES;
  tab_helper_->RequestToLaunchApp(GURL("valid://1234"), GURL::EmptyGURL(),
                                  false);
  EXPECT_EQ(1U, delegate_.countOfAlertsShown);
  EXPECT_EQ(1U, delegate_.countOfAppsLaunched);
  EXPECT_EQ(GURL("valid://1234"), delegate_.lastLaunchedAppURL);
}

// Tests that a valid URL does not launch app when launch policy is to prompt
// and user rejects.
TEST_F(AppLauncherTabHelperTest, ValidUrlPromptUserRejects) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  delegate_.simulateUserAcceptingPrompt = NO;
  tab_helper_->RequestToLaunchApp(GURL("valid://1234"), GURL::EmptyGURL(),
                                  false);
  EXPECT_EQ(0U, delegate_.countOfAppsLaunched);
}

// Tests that ShouldAllowRequest only allows requests for App Urls in main
// frame, or iframe when there was a recent user interaction.
TEST_F(AppLauncherTabHelperTest, ShouldAllowRequestWithAppUrl) {
  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  EXPECT_TRUE(VerifyRequestAllowed(url_string, /*target_frame_is_main=*/true,
                                   /*has_user_gesture=*/false));
  EXPECT_TRUE(VerifyRequestAllowed(url_string, /*target_frame_is_main=*/true,
                                   /*has_user_gesture=*/true));
  EXPECT_FALSE(VerifyRequestAllowed(url_string, /*target_frame_is_main=*/false,
                                    /*has_user_gesture=*/false));
  EXPECT_TRUE(VerifyRequestAllowed(url_string, /*target_frame_is_main=*/false,
                                   /*has_user_gesture=*/true));
}

// Tests that ShouldAllowRequest always allows requests for non App Urls.
TEST_F(AppLauncherTabHelperTest, ShouldAllowRequestWithNonAppUrl) {
  EXPECT_TRUE(VerifyRequestAllowed(
      @"http://itunes.apple.com/us/app/appname/id123",
      /*target_frame_is_main=*/true, /*has_user_gesture=*/false));
  EXPECT_TRUE(VerifyRequestAllowed(@"file://a/b/c",
                                   /*target_frame_is_main=*/true,
                                   /*has_user_gesture=*/true));
  EXPECT_TRUE(VerifyRequestAllowed(@"about://test",
                                   /*target_frame_is_main=*/false,
                                   /*has_user_gesture=*/false));
  EXPECT_TRUE(VerifyRequestAllowed(@"data://test",
                                   /*target_frame_is_main=*/false,
                                   /*has_user_gesture=*/true));
}
