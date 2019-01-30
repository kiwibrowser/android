// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/performance_measurement_manager.h"
#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "chrome/browser/resource_coordinator/tab_helper.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

class LenientMockRenderProcessProbe : public RenderProcessProbe {
 public:
  MOCK_METHOD0(StartGatherCycle, void());
  MOCK_METHOD0(StartSingleGather, void());
};
using MockRenderProcessProbe =
    testing::StrictMock<LenientMockRenderProcessProbe>;

class PerformanceMeasurementManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    ResourceCoordinatorTabHelper::CreateForWebContents(contents.get());
    return contents;
  }

  MockRenderProcessProbe& mock_render_process_probe() {
    return mock_render_process_probe_;
  }

 private:
  MockRenderProcessProbe mock_render_process_probe_;
};

TEST_F(PerformanceMeasurementManagerTest, NoMeasurementOnCreation) {
  PerformanceMeasurementManager performance_measurement_manager(
      TabLoadTracker::Get(), &mock_render_process_probe());

  auto contents = CreateWebContents();
}

TEST_F(PerformanceMeasurementManagerTest, StartMeasurementOnLoaded) {
  PerformanceMeasurementManager performance_measurement_manager(
      TabLoadTracker::Get(), &mock_render_process_probe());

  auto contents = CreateWebContents();

  EXPECT_CALL(mock_render_process_probe(), StartSingleGather());
  TabLoadTracker::Get()->TransitionStateForTesting(
      contents.get(), TabLoadTracker::LoadingState::LOADED);
}

}  // namespace resource_coordinator
