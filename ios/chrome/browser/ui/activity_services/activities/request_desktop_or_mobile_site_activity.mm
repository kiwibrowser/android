// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/request_desktop_or_mobile_site_activity.h"

#import "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kRequestDesktopOrMobileSiteActivityType =
    @"com.google.chrome.requestDesktopOrMobileSiteActivity";

}  // namespace

@interface RequestDesktopOrMobileSiteActivity ()

// The dispatcher that handles the command when the activity is performed.
@property(nonatomic, weak) id<BrowserCommands> dispatcher;
// User agent type of the current page.
@property(nonatomic, assign) web::UserAgentType userAgent;

@end

@implementation RequestDesktopOrMobileSiteActivity

@synthesize dispatcher = _dispatcher;
@synthesize userAgent = _userAgent;

+ (NSString*)activityIdentifier {
  return kRequestDesktopOrMobileSiteActivityType;
}

- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher
                         userAgent:(web::UserAgentType)userAgent {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
    _userAgent = userAgent;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return [[self class] activityIdentifier];
}

- (NSString*)activityTitle {
  if (self.userAgent == web::UserAgentType::MOBILE)
    return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_DESKTOP_SITE);
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_REQUEST_MOBILE_SITE);
}

- (UIImage*)activityImage {
  if (self.userAgent == web::UserAgentType::MOBILE)
    return [UIImage imageNamed:@"activity_services_request_desktop_site"];
  return [UIImage imageNamed:@"activity_services_request_mobile_site"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  if (self.userAgent == web::UserAgentType::MOBILE) {
    [self.dispatcher requestDesktopSite];
  } else {
    [self.dispatcher requestMobileSite];
  }
  [self activityDidFinish:YES];
}

@end
