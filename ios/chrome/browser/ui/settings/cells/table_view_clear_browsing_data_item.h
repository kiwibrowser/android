// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_TABLE_VIEW_CLEAR_BROWSING_DATA_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_TABLE_VIEW_CLEAR_BROWSING_DATA_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

enum class BrowsingDataRemoveMask;

// TableViewClearBrowsingDataItem contains the model data for a
// TableViewTextCell in addition a BrowsingDataRemoveMask property.
@interface TableViewClearBrowsingDataItem : TableViewItem

// Text of the TableViewTextCell
@property(nonatomic, copy) NSString* text;

// Whether or not the cell should show a checkmark.
@property(nonatomic, assign) BOOL checked;

// Mask of the data to be cleared.
@property(nonatomic, assign) BrowsingDataRemoveMask dataTypeMask;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_TABLE_VIEW_CLEAR_BROWSING_DATA_ITEM_H_
