// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

class BloatedRendererTabHelperTest : public ChromeRenderViewHostTestHarness {};

TEST_F(BloatedRendererTabHelperTest, DetectReload) {
  BloatedRendererTabHelper::CreateForWebContents(web_contents());
  BloatedRendererTabHelper* helper =
      BloatedRendererTabHelper::FromWebContents(web_contents());
  EXPECT_FALSE(helper->reloading_bloated_renderer_);
  helper->WillReloadPageWithBloatedRenderer();
  EXPECT_TRUE(helper->reloading_bloated_renderer_);
  helper->DidFinishNavigation(nullptr);
  EXPECT_FALSE(helper->reloading_bloated_renderer_);
}
