// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_LINK_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_LINK_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

class GURL;
@class TableViewTextLinkCell;

// Delegate for TableViewTextLinkCell.
@protocol TableViewTextLinkCellDelegate<NSObject>
// Notifies the delegate that |URL| should be opened.
- (void)tableViewTextLinkCell:(TableViewTextLinkCell*)cell
            didRequestOpenURL:(const GURL&)URL;
@end

// TableViewTextLinkItem contains the model data for a TableViewTextLinkCell.
@interface TableViewTextLinkItem : TableViewItem
// Text being stored by this item.
@property(nonatomic, readwrite, strong) NSString* text;
// URL link being stored by this item.
@property(nonatomic, assign) GURL linkURL;
@end

// UITableViewCell that displays a text label that might contain a link.
@interface TableViewTextLinkCell : UITableViewCell
// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;
// Delegate for the TableViewTextLinkCell. Is notified when a link is
// tapped.
@property(nonatomic, weak) id<TableViewTextLinkCellDelegate> delegate;
// Sets the |URL| link on the cell's label.
- (void)setLinkURL:(const GURL&)URL;
@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_LINK_ITEM_H_
