// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/bloated_renderer_detector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class BloatedRendererDetectorTest : public testing::Test {
 public:
  static TimeDelta GetMockLargeUptime() {
    return TimeDelta::FromMinutes(
        BloatedRendererDetector::kMinimumUptimeInMinutes + 1);
  }

  static TimeDelta GetMockSmallUptime() {
    return TimeDelta::FromMinutes(
        BloatedRendererDetector::kMinimumUptimeInMinutes - 1);
  }
};

TEST_F(BloatedRendererDetectorTest, ForwardToBrowser) {
  WTF::ScopedMockClock clock;
  clock.Advance(GetMockLargeUptime());
  BloatedRendererDetector detector(TimeTicks{});
  EXPECT_EQ(NearV8HeapLimitHandling::kForwardedToBrowser,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
}

TEST_F(BloatedRendererDetectorTest, SmallUptime) {
  WTF::ScopedMockClock clock;
  clock.Advance(GetMockSmallUptime());
  BloatedRendererDetector detector(TimeTicks{});
  EXPECT_EQ(NearV8HeapLimitHandling::kIgnoredDueToSmallUptime,
            detector.OnNearV8HeapLimitOnMainThreadImpl());
}

}  // namespace blink
