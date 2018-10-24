// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class GoogleServicesSettingsCoordinator;

// Delegate for GoogleServicesSettingsCoordinator.
@protocol GoogleServicesSettingsCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)googleServicesSettingsCoordinatorDidRemove:
    (GoogleServicesSettingsCoordinator*)coordinator;

@end

// Coordinator for the Google services settings view.
@interface GoogleServicesSettingsCoordinator : ChromeCoordinator

// View controller for the Google services settings.
@property(nonatomic, strong) UIViewController* viewController;

// Delegate.
@property(nonatomic, weak) id<GoogleServicesSettingsCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_COORDINATOR_H_
