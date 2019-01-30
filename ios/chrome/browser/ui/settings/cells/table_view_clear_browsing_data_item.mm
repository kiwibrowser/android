// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TableViewClearBrowsingDataItem
@synthesize checked = _checked;
@synthesize dataTypeMask = _dataTypeMask;
@synthesize text = _text;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextCell* cell =
      base::mac::ObjCCastStrict<TableViewTextCell>(tableCell);
  cell.textLabel.text = self.text;
  cell.textLabel.textColor = [UIColor blackColor];
  cell.checked = self.checked;
  cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
}

@end
