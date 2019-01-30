// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"

#include <memory>

#include "base/strings/string_util.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/common/page_load_metrics/test/page_load_metrics_test_util.h"

class SchemePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<SchemePageLoadMetricsObserver> observer =
        std::make_unique<SchemePageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_timing->parse_start = base::TimeDelta::FromMilliseconds(100);
    timing->paint_timing->first_paint = base::TimeDelta::FromMilliseconds(200);
    timing->paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(300);
    timing->paint_timing->first_meaningful_paint =
        base::TimeDelta::FromMilliseconds(400);
    timing->document_timing->dom_content_loaded_event_start =
        base::TimeDelta::FromMilliseconds(600);
    timing->document_timing->load_event_start =
        base::TimeDelta::FromMilliseconds(1000);
    PopulateRequiredTimingFields(timing);
  }

  void SimulateNavigation(std::string scheme) {
    NavigateAndCommit(GURL(scheme.append("://google.com")));

    page_load_metrics::mojom::PageLoadTiming timing;
    InitializeTestPageLoadTiming(&timing);
    SimulateTimingUpdate(timing);

    // Navigate again to force OnComplete, which happens when a new navigation
    // occurs.
    NavigateAndCommit(GURL(scheme.append("://example.com")));
  }

  int CountTotalProtocolMetricsRecorded() {
    int count = 0;

    base::HistogramTester::CountsMap counts_map =
        histogram_tester().GetTotalCountsForPrefix("PageLoad.Clients.Scheme.");
    for (const auto& entry : counts_map)
      count += entry.second;
    return count;
  }

  void CheckHistograms(int expected_count, const std::string& protocol) {
    EXPECT_EQ(expected_count, CountTotalProtocolMetricsRecorded());
    if (expected_count == 0)
      return;

    std::string prefix = "PageLoad.Clients.Scheme.";
    prefix += base::ToUpperASCII(protocol);

    histogram_tester().ExpectTotalCount(
        prefix + ".ParseTiming.NavigationToParseStart", 1);
    histogram_tester().ExpectTotalCount(
        prefix + ".PaintTiming.NavigationToFirstContentfulPaint", 1);
    histogram_tester().ExpectTotalCount(
        prefix + ".Experimental.PaintTiming.NavigationToFirstMeaningfulPaint",
        1);
  }

  SchemePageLoadMetricsObserver* observer_;
};

TEST_F(SchemePageLoadMetricsObserverTest, HTTPNavigation) {
  SimulateNavigation(url::kHttpScheme);
  CheckHistograms(3, url::kHttpScheme);
}

TEST_F(SchemePageLoadMetricsObserverTest, HTTPSNavigation) {
  SimulateNavigation(url::kHttpsScheme);
  CheckHistograms(3, url::kHttpsScheme);
}

// Make sure no metrics are recorded for an unobserved scheme.
TEST_F(SchemePageLoadMetricsObserverTest, AboutNavigation) {
  SimulateNavigation(url::kAboutScheme);
  CheckHistograms(0, "");
}
