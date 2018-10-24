// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_tab_helper.h"

#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::StrictMock;

namespace predictors {

class MockLoadingDataCollector : public LoadingDataCollector {
 public:
  explicit MockLoadingDataCollector(const LoadingPredictorConfig& config);

  MOCK_METHOD1(RecordStartNavigation, void(const NavigationID&));
  MOCK_METHOD3(RecordFinishNavigation,
               void(const NavigationID&, const NavigationID&, bool));
  MOCK_METHOD2(RecordResourceLoadComplete,
               void(const NavigationID&,
                    const content::mojom::ResourceLoadInfo&));
  MOCK_METHOD1(RecordMainFrameLoadComplete, void(const NavigationID&));
  MOCK_METHOD2(RecordFirstContentfulPaint,
               void(const NavigationID&, const base::TimeTicks&));
};

MockLoadingDataCollector::MockLoadingDataCollector(
    const LoadingPredictorConfig& config)
    : LoadingDataCollector(nullptr, nullptr, config) {}

class LoadingPredictorTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;
  void TearDown() override;

  void ExpectRecordNavigation(const std::string& url);
  SessionID GetTabID();
  void NavigateAndCommitInFrame(const std::string& url,
                                content::RenderFrameHost* rfh);

 protected:
  std::unique_ptr<LoadingPredictor> loading_predictor_;
  // Owned by |loading_predictor_|.
  StrictMock<MockLoadingDataCollector>* mock_collector_;
  // Owned by |web_contents()|.
  LoadingPredictorTabHelper* tab_helper_;
};

void LoadingPredictorTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SessionTabHelper::CreateForWebContents(web_contents());
  LoadingPredictorTabHelper::CreateForWebContents(web_contents());
  tab_helper_ = LoadingPredictorTabHelper::FromWebContents(web_contents());

  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  loading_predictor_ = std::make_unique<LoadingPredictor>(config, profile());

  auto mock_collector =
      std::make_unique<StrictMock<MockLoadingDataCollector>>(config);
  mock_collector_ = mock_collector.get();
  loading_predictor_->set_mock_loading_data_collector(
      std::move(mock_collector));

  tab_helper_->SetLoadingPredictorForTesting(loading_predictor_->GetWeakPtr());
}

void LoadingPredictorTabHelperTest::TearDown() {
  loading_predictor_->Shutdown();
  ChromeRenderViewHostTestHarness::TearDown();
}

void LoadingPredictorTabHelperTest::ExpectRecordNavigation(
    const std::string& url) {
  auto navigation_id = CreateNavigationID(GetTabID(), url);
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(navigation_id));
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(navigation_id, navigation_id,
                                     /* is_error_page */ false));
}

SessionID LoadingPredictorTabHelperTest::GetTabID() {
  return SessionTabHelper::IdForTab(web_contents());
}

void LoadingPredictorTabHelperTest::NavigateAndCommitInFrame(
    const std::string& url,
    content::RenderFrameHost* rfh) {
  auto navigation =
      content::NavigationSimulator::CreateRendererInitiated(GURL(url), rfh);
  navigation->Start();
  navigation->Commit();
}

// Tests that a main frame navigation is correctly recorded by the
// LoadingDataCollector.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigation) {
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());
}

// Tests that an old and new navigation ids are correctly set if a navigation
// has redirects.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigationWithRedirects) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.org"), main_rfh());
  auto navigation_id = CreateNavigationID(GetTabID(), "http://test.org");
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(navigation_id));
  navigation->Start();
  navigation->Redirect(GURL("http://test2.org"));
  navigation->Redirect(GURL("http://test3.org"));
  auto new_navigation_id = CreateNavigationID(GetTabID(), "http://test3.org");
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(navigation_id, new_navigation_id,
                                     /* is_error_page */ false));
  navigation->Commit();
}

// Tests that a subframe navigation is not recorded.
TEST_F(LoadingPredictorTabHelperTest, SubframeNavigation) {
  // We need to have a committed main frame navigation before navigating in sub
  // frames.
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  // Subframe navigation shouldn't be recorded.
  NavigateAndCommitInFrame("http://sub.test.org", subframe);
}

// Tests that a failed navigation is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigationFailed) {
  auto navigation = content::NavigationSimulator::CreateRendererInitiated(
      GURL("http://test.org"), main_rfh());
  auto navigation_id = CreateNavigationID(GetTabID(), "http://test.org");
  EXPECT_CALL(*mock_collector_, RecordStartNavigation(navigation_id));
  EXPECT_CALL(*mock_collector_,
              RecordFinishNavigation(navigation_id, navigation_id,
                                     /* is_error_page */ true));
  navigation->Start();
  navigation->Fail(net::ERR_TIMED_OUT);
  navigation->CommitErrorPage();
}

// Tests that a same document navigation is not recorded.
TEST_F(LoadingPredictorTabHelperTest, MainFrameNavigationSameDocument) {
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  // Same document navigation shouldn't be recorded.
  content::NavigationSimulator::CreateRendererInitiated(GURL("http://test.org"),
                                                        main_rfh())
      ->CommitSameDocument();
}

// Tests that document on load completed is recorded with correct navigation
// id.
TEST_F(LoadingPredictorTabHelperTest, DocumentOnLoadCompleted) {
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  // Adding subframe navigation to ensure that the commited main frame url will
  // be used.
  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  auto navigation_id = CreateNavigationID(GetTabID(), "http://test.org");
  EXPECT_CALL(*mock_collector_, RecordMainFrameLoadComplete(navigation_id));
  tab_helper_->DocumentOnLoadCompletedInMainFrame();
}

// Tests that a resource load is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, ResourceLoadComplete) {
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  auto navigation_id = CreateNavigationID(GetTabID(), "http://test.org");
  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", content::RESOURCE_TYPE_SCRIPT);
  EXPECT_CALL(*mock_collector_,
              RecordResourceLoadComplete(navigation_id,
                                         Eq(ByRef(*resource_load_info))));
  tab_helper_->ResourceLoadComplete(main_rfh(), *resource_load_info);
}

// Tests that a resource loaded in a subframe is not recorded.
TEST_F(LoadingPredictorTabHelperTest, ResourceLoadCompleteInSubFrame) {
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  auto* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  NavigateAndCommitInFrame("http://sub.test.org", subframe);

  // Resource loaded in subframe shouldn't be recorded.
  auto resource_load_info = CreateResourceLoadInfo(
      "http://sub.test.org/script.js", content::RESOURCE_TYPE_SCRIPT);
  tab_helper_->ResourceLoadComplete(subframe, *resource_load_info);
}

// Tests that a resource load from the memory cache is correctly recorded.
TEST_F(LoadingPredictorTabHelperTest, LoadResourceFromMemoryCache) {
  ExpectRecordNavigation("http://test.org");
  NavigateAndCommitInFrame("http://test.org", main_rfh());

  auto navigation_id = CreateNavigationID(GetTabID(), "http://test.org");
  auto resource_load_info = CreateResourceLoadInfo(
      "http://test.org/script.js", content::RESOURCE_TYPE_SCRIPT, false);
  resource_load_info->mime_type = "application/javascript";
  resource_load_info->network_info->network_accessed = false;
  EXPECT_CALL(*mock_collector_,
              RecordResourceLoadComplete(navigation_id,
                                         Eq(ByRef(*resource_load_info))));
  tab_helper_->DidLoadResourceFromMemoryCache(
      GURL("http://test.org/script.js"), "application/javascript",
      content::ResourceType::RESOURCE_TYPE_SCRIPT);
}

}  // namespace predictors
