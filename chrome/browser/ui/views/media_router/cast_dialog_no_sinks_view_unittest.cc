// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_no_sinks_view.h"

#include <memory>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class CastDialogNoSinksViewTest : public ChromeViewsTestBase {
 public:
  CastDialogNoSinksViewTest() = default;
  ~CastDialogNoSinksViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    no_sinks_view_ = std::make_unique<CastDialogNoSinksView>(nullptr);
  }

 protected:
  views::View* looking_for_sinks_view() {
    return no_sinks_view_->looking_for_sinks_view_for_test();
  }
  views::View* help_icon_view() {
    return no_sinks_view_->help_icon_view_for_test();
  }

 private:
  content::TestBrowserThreadBundle test_thread_bundle_;
  std::unique_ptr<CastDialogNoSinksView> no_sinks_view_;

  DISALLOW_COPY_AND_ASSIGN(CastDialogNoSinksViewTest);
};

TEST_F(CastDialogNoSinksViewTest, SwitchViews) {
  // Initially, only the throbber view should be shown.
  EXPECT_TRUE(looking_for_sinks_view()->visible());
  EXPECT_FALSE(help_icon_view());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(3));
  run_loop.Run();
  // After three seconds, only the help icon view should be shown.
  EXPECT_FALSE(looking_for_sinks_view());
  EXPECT_TRUE(help_icon_view()->visible());
}

}  // namespace media_router
