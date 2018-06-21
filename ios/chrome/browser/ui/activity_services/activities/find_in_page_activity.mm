// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"

#import "ios/chrome/browser/ui/commands/browser_commands.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kFindInPageActivityType =
    @"com.google.chrome.FindInPageActivityType";

}  // namespace

@interface FindInPageActivity ()
// The dispatcher that handles the command when the activity is performed.
@property(nonatomic, weak) id<BrowserCommands> dispatcher;
@end

@implementation FindInPageActivity

@synthesize dispatcher = _dispatcher;

+ (NSString*)activityIdentifier {
  return kFindInPageActivityType;
}

- (instancetype)initWithDispatcher:(id<BrowserCommands>)dispatcher {
  self = [super init];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return [[self class] activityIdentifier];
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_FIND_IN_PAGE);
}

- (UIImage*)activityImage {
  return [UIImage imageNamed:@"activity_services_find_in_page"];
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return YES;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self.dispatcher showFindInPage];
  [self activityDidFinish:YES];
}

@end
