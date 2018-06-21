// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_item.h"

#include "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ClearBrowsingDataItem {
  // Wrapper around the data volume counter associated with the item. May be
  // null if there is no counter or if the feature kNewClearBrowsingDataUI
  // is disabled.
  std::unique_ptr<BrowsingDataCounterWrapper> _counter;
}

@synthesize dataTypeMask = _dataTypeMask;
@synthesize prefName = _prefName;

- (instancetype)initWithType:(NSInteger)type
                     counter:
                         (std::unique_ptr<BrowsingDataCounterWrapper>)counter {
  if ((self = [super initWithType:type])) {
    _counter = std::move(counter);
  }
  return self;
}

- (void)restartCounter {
  if (_counter)
    _counter->RestartCounter();
}

#pragma mark - Properties

- (BOOL)hasCounter {
  return _counter != nullptr;
}

@end
