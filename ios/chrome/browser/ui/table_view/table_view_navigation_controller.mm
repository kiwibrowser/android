// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewNavigationController
@synthesize tableViewController = _tableViewController;

#pragma mark - Public Interface

- (instancetype)initWithTable:(ChromeTableViewController*)table {
  self = [super initWithRootViewController:table];
  if (self) {
    _tableViewController = table;
  }
  return self;
}

#pragma mark - View Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];

  UIVisualEffectView* visualEffectView = [[UIVisualEffectView alloc]
      initWithEffect:[UIBlurEffect
                         effectWithStyle:UIBlurEffectStyleExtraLight]];

  self.navigationBar.translucent = YES;
  self.navigationController.navigationBar.backgroundColor =
      [UIColor clearColor];
  [self.navigationController.navigationBar addSubview:visualEffectView];
  if (@available(iOS 11, *))
    self.navigationBar.prefersLargeTitles = YES;

  [self.toolbar setShadowImage:[UIImage new]
            forToolbarPosition:UIBarPositionAny];
  self.toolbar.translucent = YES;
  [self.toolbar addSubview:visualEffectView];
}

@end
