// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/low_memory_mode_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class LowMemoryModeControllerTest : public testing::Test {
 public:
  LowMemoryModeController* controller() { return &controller_; }

  void ExpectTransitionCount(int enabled_count, int disabled_count) {
    static constexpr char kHistogram[] =
        "SiteIsolation.LowMemoryMode.Transition";
    histogram_tester_.ExpectBucketCount(kHistogram, true, enabled_count);
    histogram_tester_.ExpectBucketCount(kHistogram, false, disabled_count);
    histogram_tester_.ExpectTotalCount(kHistogram,
                                       enabled_count + disabled_count);
  }

 private:
  base::HistogramTester histogram_tester_;
  LowMemoryModeController controller_;
};

TEST_F(LowMemoryModeControllerTest, CreateMainFrames) {
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameCreated(true);
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameCreated(true);
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameDestroyed(true);
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameDestroyed(true);
  EXPECT_TRUE(controller()->is_enabled());

  ExpectTransitionCount(1, 0);
}

TEST_F(LowMemoryModeControllerTest, MainFrameAddSubframe) {
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameCreated(true);
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameCreated(false);
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameDestroyed(true);
  EXPECT_TRUE(controller()->is_enabled());

  controller()->OnFrameDestroyed(false);
  EXPECT_TRUE(controller()->is_enabled());

  ExpectTransitionCount(1, 0);
}

TEST_F(LowMemoryModeControllerTest, SubFrameAddMainFrame) {
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameCreated(false);
  EXPECT_TRUE(controller()->is_enabled());

  controller()->OnFrameCreated(true);
  EXPECT_FALSE(controller()->is_enabled());

  controller()->OnFrameDestroyed(true);
  EXPECT_TRUE(controller()->is_enabled());

  controller()->OnFrameDestroyed(false);
  EXPECT_TRUE(controller()->is_enabled());

  ExpectTransitionCount(2, 1);
}

}  // namespace
}  // namespace content
