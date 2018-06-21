// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/fake_js_autofill_manager.h"

#include "base/bind.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeJSAutofillManager

@synthesize lastClearedFormName = _lastClearedFormName;
@synthesize lastClearedFieldIdentifier = _lastClearedFieldIdentifier;

- (void)clearAutofilledFieldsForFormName:(NSString*)formName
                         fieldIdentifier:(NSString*)fieldIdentifier
                       completionHandler:(ProceduralBlock)completionHandler {
  web::WebThread::PostTask(web::WebThread::UI, FROM_HERE, base::BindOnce(^{
                             _lastClearedFormName = [formName copy];
                             _lastClearedFieldIdentifier =
                                 [fieldIdentifier copy];
                             completionHandler();
                           }));
}

@end
