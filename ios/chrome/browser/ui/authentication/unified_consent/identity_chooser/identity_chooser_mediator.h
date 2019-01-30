// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class ChromeIdentity;
@class IdentityChooserViewController;

// A mediator object that monitors updates of chrome identities, and updates the
// IdentityChooserViewController.
@interface IdentityChooserMediator : NSObject

// Selected Chrome identity.
@property(nonatomic, strong) ChromeIdentity* selectedIdentity;
// View controller.
@property(nonatomic, weak)
    IdentityChooserViewController* identityChooserViewController;

// Starts this mediator.
- (void)start;

// Selects an identity with a Gaia ID.
- (void)selectIdentityWithGaiaID:(NSString*)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_MEDIATOR_H_
