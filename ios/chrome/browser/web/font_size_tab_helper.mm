// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/font_size_tab_helper.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(FontSizeTabHelper);

FontSizeTabHelper::~FontSizeTabHelper() {
  // Remove observer in destructor because |this| is captured by the usingBlock
  // in calling [NSNotificationCenter.defaultCenter
  // addObserverForName:object:queue:usingBlock] in constructor.
  [NSNotificationCenter.defaultCenter
      removeObserver:content_size_did_change_observer_];
}

FontSizeTabHelper::FontSizeTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  content_size_did_change_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIContentSizeCategoryDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* _Nonnull note) {
                SetPageFontSize(GetSystemSuggestedFontSize());
              }];
}

void FontSizeTabHelper::SetPageFontSize(int size) {
  if (web_state_->ContentIsHTML()) {
    NSString* js = [NSString
        stringWithFormat:@"__gCrWeb.accessibility.adjustFontSize(%d)", size];
    web_state_->ExecuteJavaScript(base::SysNSStringToUTF16(js));
  }
}

int FontSizeTabHelper::GetSystemSuggestedFontSize() const {
  // TODO(crbug.com/836962): Determine precise scaling numbers for
  // |font_size_map|.
  static NSDictionary* font_size_map = @{
    UIContentSizeCategoryUnspecified : @100,
    UIContentSizeCategoryExtraSmall : @70,
    UIContentSizeCategorySmall : @80,
    UIContentSizeCategoryMedium : @90,
    UIContentSizeCategoryLarge : @100,  // system default
    UIContentSizeCategoryExtraLarge : @110,
    UIContentSizeCategoryExtraExtraLarge : @120,
    UIContentSizeCategoryExtraExtraExtraLarge : @130,
    UIContentSizeCategoryAccessibilityMedium : @140,
    UIContentSizeCategoryAccessibilityLarge : @150,
    UIContentSizeCategoryAccessibilityExtraLarge : @160,
    UIContentSizeCategoryAccessibilityExtraExtraLarge : @170,
    UIContentSizeCategoryAccessibilityExtraExtraExtraLarge : @180,
  };
  UIContentSizeCategory category =
      UIApplication.sharedApplication.preferredContentSizeCategory;
  NSNumber* font_size = font_size_map[category];
  return font_size ? font_size.intValue : 100;
}

void FontSizeTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void FontSizeTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state, web_state_);
  int size = GetSystemSuggestedFontSize();
  if (size != 100)
    SetPageFontSize(size);
}
