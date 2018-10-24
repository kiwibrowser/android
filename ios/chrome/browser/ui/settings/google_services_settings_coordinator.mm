// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_coordinator.h"

#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleServicesSettingsCoordinator ()<
    GoogleServicesSettingsViewControllerPresentationDelegate>
@end

@implementation GoogleServicesSettingsCoordinator

@synthesize delegate = _delegate;
@synthesize viewController = _viewController;

- (void)start {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  GoogleServicesSettingsViewController* controller =
      [[GoogleServicesSettingsViewController alloc]
          initWithLayout:layout
                   style:CollectionViewControllerStyleAppBar];
  controller.presentationDelegate = self;
  self.viewController = controller;
  DCHECK(self.navigationController);
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

@end
