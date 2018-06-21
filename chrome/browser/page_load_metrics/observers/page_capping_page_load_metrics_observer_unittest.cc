// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_capping_page_load_metrics_observer.h"

#include <memory>

#include "base/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/mock_infobar_service.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace {
const char kTestURL[] = "http://www.test.com";
}

// Class that overrides WriteToSavings.
class TestPageCappingPageLoadMetricsObserver
    : public PageCappingPageLoadMetricsObserver {
 public:
  using SizeUpdateCallback = base::RepeatingCallback<void(int64_t)>;
  TestPageCappingPageLoadMetricsObserver(int64_t fuzzing_offset,
                                         const SizeUpdateCallback& callback)
      : fuzzing_offset_(fuzzing_offset), size_callback_(callback) {}
  ~TestPageCappingPageLoadMetricsObserver() override {}

  void WriteToSavings(int64_t bytes_saved) override {
    size_callback_.Run(bytes_saved);
  }

  int64_t GetFuzzingOffset() const override { return fuzzing_offset_; }

 private:
  int64_t fuzzing_offset_;
  SizeUpdateCallback size_callback_;
};

class PageCappingObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PageCappingObserverTest() = default;
  ~PageCappingObserverTest() override = default;

  void SetUpTest() {
    MockInfoBarService::CreateForWebContents(web_contents());
    NavigateAndCommit(GURL(kTestURL));
  }

  size_t InfoBarCount() { return infobar_service()->infobar_count(); }

  void RemoveAllInfoBars() { infobar_service()->RemoveAllInfoBars(false); }

  InfoBarService* infobar_service() {
    return InfoBarService::FromWebContents(web_contents());
  }

  // Called from the observer when |WriteToSavings| is called.
  void UpdateSavings(int64_t savings) { savings_ += savings; }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    auto observer = std::make_unique<TestPageCappingPageLoadMetricsObserver>(
        fuzzing_offset_,
        base::BindRepeating(&PageCappingObserverTest::UpdateSavings,
                            base::Unretained(this)));
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }
  int64_t savings_ = 0;
  int64_t fuzzing_offset_ = 0;
  TestPageCappingPageLoadMetricsObserver* observer_;
};

TEST_F(PageCappingObserverTest, ExperimentDisabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndDisableFeature(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages);
  SetUpTest();

  // A resource slightly over 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) + 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // The infobar should not show even though the cap would be met because the
  // feature is disabled.
  SimulateLoadedResource(resource);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdNotMetNonMedia) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      {});
  SetUpTest();

  // A resource slightly under 5 MB, the default page capping threshold.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (5 * 1024 * 1024) - 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // The cap is not met, so the infobar should not show.
  SimulateLoadedResource(resource);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdMetNonMedia) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      {});
  SetUpTest();

  // A resource slightly over 5 MB, the default page capping threshold.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (5 * 1024 * 1024) + 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // The cap is not met, so the infobar should not show.
  SimulateLoadedResource(resource);

  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdNotMetMedia) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      {});
  SetUpTest();

  SimulateMediaPlayed();

  // A resource slightly under 15 MB, the default media page capping threshold.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (15 * 1024 * 1024) - 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // The cap is not met, so the infobar should not show.
  SimulateLoadedResource(resource);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, DefaultThresholdMetMedia) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      {});
  SetUpTest();

  SimulateMediaPlayed();

  // A resource slightly over 15 MB, the default media page capping threshold.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (15 * 1024 * 1024) + 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // The cap is not met, so the infobar should not show.
  SimulateLoadedResource(resource);

  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, NotEnoughForThreshold) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // A resource slightly under 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) - 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // The cap is not met, so the infobar should not show.
  SimulateLoadedResource(resource);

  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, InfobarOnlyShownOnce) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // A resource slightly over 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) + 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger the infobar.
  SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());
  // The infobar is already being shown, so this should not trigger and infobar.
  SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());

  // Clear all infobars.
  RemoveAllInfoBars();
  // Verify the infobars are clear.
  EXPECT_EQ(0u, InfoBarCount());
  // This would trigger and infobar if one was not already shown from this
  // observer.
  SimulateLoadedResource(resource);
  EXPECT_EQ(0u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, MediaCap) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "10"}, {"PageCapMiB", "1"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // Show that media has played.
  SimulateMediaPlayed();

  // A resource slightly under 10 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (10 * 1024 * 1024) - 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should not trigger an infobar as the media cap is not met.
  SimulateLoadedResource(resource);
  EXPECT_EQ(0u, InfoBarCount());
  // Adding more data should now trigger the infobar.
  SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, PageCap) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"}, {"PageCapMiB", "10"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // A resource slightly under 10 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (10 * 1024 * 1024) - 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should not trigger an infobar as the non-media cap is not met.
  SimulateLoadedResource(resource);
  EXPECT_EQ(0u, InfoBarCount());
  // Adding more data should now trigger the infobar.
  SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());
}

TEST_F(PageCappingObserverTest, PageCappingTriggered) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // A resource slightly over 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) + 10 /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());

  // Verify the callback is called twice with appropriate bool values.
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->GetPauseSubresourceLoadingCalled());
  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_TRUE(observer_->IsPausedForTesting());

  content::WebContentsTester::For(web_contents())
      ->ResetPauseSubresourceLoadingCalled();
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  EXPECT_FALSE(observer_->IsPausedForTesting());
}

// Check that data savings works without a specific param. The estimated page
// size should be 1.5 the threshold.
TEST_F(PageCappingObserverTest, DataSavingsDefault) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"}, {"PageCapMiB", "1"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // A resource of 1/4 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 256) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  for (size_t i = 0; i < 4; i++)
    SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());

  // Verify the callback is called twice with appropriate bool values.
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->GetPauseSubresourceLoadingCalled());
  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_TRUE(observer_->IsPausedForTesting());

  // This should cause savings to be written.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024 / 2, savings_);

  SimulateLoadedResource(resource);

  // Adding another resource and forcing savings to be written should reduce
  // total savings.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024 / 4, savings_);

  content::WebContentsTester::For(web_contents())
      ->ResetPauseSubresourceLoadingCalled();
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  EXPECT_FALSE(observer_->IsPausedForTesting());

  // Resuming the page should force savings to 0.
  NavigateToUntrackedUrl();
  EXPECT_EQ(0l, savings_);
}

// Check that data savings works with a specific param. The estimated page size
// should be |PageTypicalLargePageMB|.
TEST_F(PageCappingObserverTest, DataSavingsParam) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();

  // A resource of 1/4 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 256) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  for (size_t i = 0; i < 4; i++)
    SimulateLoadedResource(resource);
  EXPECT_EQ(1u, InfoBarCount());

  // Verify the callback is called twice with appropriate bool values.
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WebContentsTester::For(web_contents())
                  ->GetPauseSubresourceLoadingCalled());
  EXPECT_EQ(1u, InfoBarCount());
  EXPECT_TRUE(observer_->IsPausedForTesting());

  // This should cause savings to be written.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024, savings_);

  SimulateLoadedResource(resource);

  // Adding another resource and forcing savings to be written should reduce
  // total savings.
  SimulateAppEnterBackground();
  EXPECT_EQ(1024 * 1024 * 3 / 4, savings_);

  content::WebContentsTester::For(web_contents())
      ->ResetPauseSubresourceLoadingCalled();
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(content::WebContentsTester::For(web_contents())
                   ->GetPauseSubresourceLoadingCalled());
  EXPECT_FALSE(observer_->IsPausedForTesting());

  // Resuming should make the savings 0.
  SimulateAppEnterBackground();
  EXPECT_EQ(0l, savings_);

  // Forcing savings to be written again should not change savings.
  NavigateToUntrackedUrl();
  EXPECT_EQ(0l, savings_);
}

TEST_F(PageCappingObserverTest, DataSavingsHistogram) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();
  base::HistogramTester histogram_tester;

  // A resource of 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  SimulateLoadedResource(resource);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // Savings is only recorded in OnComplete
  SimulateAppEnterBackground();
  histogram_tester.ExpectTotalCount("HeavyPageCapping.RecordedDataSavings", 0);

  NavigateToUntrackedUrl();
  histogram_tester.ExpectUniqueSample("HeavyPageCapping.RecordedDataSavings",
                                      1024, 1);
}

TEST_F(PageCappingObserverTest, DataSavingsHistogramWhenResumed) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();
  base::HistogramTester histogram_tester;

  // A resource of 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  SimulateLoadedResource(resource);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  NavigateToUntrackedUrl();
  histogram_tester.ExpectTotalCount("HeavyPageCapping.RecordedDataSavings", 0);
}

TEST_F(PageCappingObserverTest, UKMNotRecordedWhenNotTriggered) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();
  base::HistogramTester histogram_tester;
  NavigateToUntrackedUrl();

  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(0u, entries.size());
}

TEST_F(PageCappingObserverTest, UKMRecordedInfoBarShown) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();
  base::HistogramTester histogram_tester;

  // A resource of 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  SimulateLoadedResource(resource);

  NavigateToUntrackedUrl();

  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestURL));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(entries[0],
                                                 UkmEntry::kFinalStateName));
  EXPECT_EQ(static_cast<int64_t>(1),
            *(test_ukm_recorder().GetEntryMetric(entries[0],
                                                 UkmEntry::kFinalStateName)));
  EXPECT_EQ(
      static_cast<int64_t>(
          PageCappingPageLoadMetricsObserver::PageCappingState::kInfoBarShown),
      *(test_ukm_recorder().GetEntryMetric(entries[0],
                                           UkmEntry::kFinalStateName)));
}

TEST_F(PageCappingObserverTest, UKMRecordedPaused) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();
  base::HistogramTester histogram_tester;

  // A resource of 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  SimulateLoadedResource(resource);

  // Pause the page.
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  NavigateToUntrackedUrl();
  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestURL));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(entries[0],
                                                 UkmEntry::kFinalStateName));
  EXPECT_EQ(static_cast<int64_t>(2),
            *(test_ukm_recorder().GetEntryMetric(entries[0],
                                                 UkmEntry::kFinalStateName)));
  EXPECT_EQ(
      static_cast<int64_t>(
          PageCappingPageLoadMetricsObserver::PageCappingState::kPagePaused),
      *(test_ukm_recorder().GetEntryMetric(entries[0],
                                           UkmEntry::kFinalStateName)));
}

TEST_F(PageCappingObserverTest, UKMRecordedResumed) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  SetUpTest();
  base::HistogramTester histogram_tester;

  // A resource of 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an infobar as the non-media cap is met.
  SimulateLoadedResource(resource);

  // Pause then resume.
  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  static_cast<ConfirmInfoBarDelegate*>(
      infobar_service()->infobar_at(0u)->delegate())
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  NavigateToUntrackedUrl();
  using UkmEntry = ukm::builders::PageLoadCapping;
  auto entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);

  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder().ExpectEntrySourceHasUrl(entries[0], GURL(kTestURL));
  EXPECT_TRUE(test_ukm_recorder().EntryHasMetric(entries[0],
                                                 UkmEntry::kFinalStateName));
  EXPECT_EQ(static_cast<int64_t>(3),
            *(test_ukm_recorder().GetEntryMetric(entries[0],
                                                 UkmEntry::kFinalStateName)));
  EXPECT_EQ(
      static_cast<int64_t>(
          PageCappingPageLoadMetricsObserver::PageCappingState::kPageResumed),
      *(test_ukm_recorder().GetEntryMetric(entries[0],
                                           UkmEntry::kFinalStateName)));
}

TEST_F(PageCappingObserverTest, FuzzingOffset) {
  std::map<std::string, std::string> feature_parameters = {
      {"MediaPageCapMiB", "1"},
      {"PageCapMiB", "1"},
      {"PageTypicalLargePageMiB", "2"}};
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      feature_parameters);
  fuzzing_offset_ = 1;
  SetUpTest();
  base::HistogramTester histogram_tester;

  // A resource of 1 MB.
  page_load_metrics::ExtraRequestCompleteInfo resource = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should not trigger an infobar as the non-media cap is not met.
  SimulateLoadedResource(resource);

  EXPECT_EQ(0u, InfoBarCount());

  // A resource of 1 KB.
  page_load_metrics::ExtraRequestCompleteInfo resource2 = {
      GURL(kTestURL),
      net::HostPortPair(),
      -1 /* frame_tree_node_id */,
      false /* was_cached */,
      (1 * 1024) /* raw_body_bytes */,
      0 /* original_network_content_length */,
      nullptr,
      content::ResourceType::RESOURCE_TYPE_SCRIPT,
      0,
      {} /* load_timing_info */};

  // This should trigger an InfoBar as the non-media cap is met.
  SimulateLoadedResource(resource2);

  EXPECT_EQ(1u, InfoBarCount());
}
