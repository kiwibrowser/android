// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_text_field_item.h"

#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using BookmarkTextFieldItemTest = PlatformTest;

TEST_F(BookmarkTextFieldItemTest,
       DelegateGetsTextFieldEventsUIRefreshDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kUIRefreshPhase1);

  BookmarkTextFieldItem* item = [[BookmarkTextFieldItem alloc] initWithType:0];
  LegacyBookmarkTextFieldCell* cell =
      [[LegacyBookmarkTextFieldCell alloc] initWithFrame:CGRectZero];
  id mockDelegate =
      [OCMockObject mockForProtocol:@protocol(BookmarkTextFieldItemDelegate)];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.delegate = mockDelegate;
  [item configureCell:cell withStyler:styler];
  EXPECT_EQ(mockDelegate, cell.textField.delegate);

  [[mockDelegate expect] textDidChangeForItem:item];
  cell.textField.text = @"Foo";
}

TEST_F(BookmarkTextFieldItemTest, DelegateGetsTextFieldEventsUIRefreshEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kUIRefreshPhase1);

  BookmarkTextFieldItem* item = [[BookmarkTextFieldItem alloc] initWithType:0];
  BookmarkTextFieldCell* cell =
      [[BookmarkTextFieldCell alloc] initWithFrame:CGRectZero];
  id mockDelegate =
      [OCMockObject mockForProtocol:@protocol(BookmarkTextFieldItemDelegate)];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.delegate = mockDelegate;
  [item configureCell:cell withStyler:styler];
  EXPECT_EQ(mockDelegate, cell.textField.delegate);

  [[mockDelegate expect] textDidChangeForItem:item];
  cell.textField.text = @"Foo";
}

TEST_F(BookmarkTextFieldItemTest, TextFieldGetsTextUIRefreshDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kUIRefreshPhase1);

  BookmarkTextFieldItem* item = [[BookmarkTextFieldItem alloc] initWithType:0];
  LegacyBookmarkTextFieldCell* cell =
      [[LegacyBookmarkTextFieldCell alloc] initWithFrame:CGRectZero];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.text = @"Foo";
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(@"Foo", cell.textField.text);
}

TEST_F(BookmarkTextFieldItemTest, TextFieldGetsTextUIRefreshEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kUIRefreshPhase1);

  BookmarkTextFieldItem* item = [[BookmarkTextFieldItem alloc] initWithType:0];
  BookmarkTextFieldCell* cell =
      [[BookmarkTextFieldCell alloc] initWithFrame:CGRectZero];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.text = @"Foo";
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(@"Foo", cell.textField.text);
}

}  // namespace
