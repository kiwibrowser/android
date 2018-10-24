// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CLEAR_BROWSING_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CLEAR_BROWSING_DATA_ITEM_H_

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"

#include <memory>

class BrowsingDataCounterWrapper;
enum class BrowsingDataRemoveMask;

// Collection view item identifying a clear browsing data content view.
@interface ClearBrowsingDataItem : CollectionViewTextItem

// Designated initializer with |counter| that can be nil.
- (instancetype)initWithType:(NSInteger)type
                     counter:
                         (std::unique_ptr<BrowsingDataCounterWrapper>)counter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

// Restarts the counter (if defined).
- (void)restartCounter;

// Mask of the data to be cleared.
@property(nonatomic, assign) BrowsingDataRemoveMask dataTypeMask;

// Pref name associated with the item.
@property(nonatomic, assign) const char* prefName;

// Whether this item has a data volume counter associated.
@property(nonatomic, readonly) BOOL hasCounter;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CLEAR_BROWSING_DATA_ITEM_H_
