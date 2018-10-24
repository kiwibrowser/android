// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_TEST_H_
#define IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_TEST_H_

#include "base/macros.h"
#import "ios/chrome/test/root_view_controller_test.h"

@protocol TabSwitcher;

class MainViewControllerTest : public RootViewControllerTest {
 public:
  MainViewControllerTest() = default;
  ~MainViewControllerTest() override = default;

 protected:
  // Creates and returns an object that conforms to the TabSwitcher protocol.
  id<TabSwitcher> CreateTestTabSwitcher();

 private:
  DISALLOW_COPY_AND_ASSIGN(MainViewControllerTest);
};

#endif  // IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_TEST_H_
