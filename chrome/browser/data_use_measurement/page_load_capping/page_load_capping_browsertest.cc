// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/simple_connection_listener.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"

namespace {
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/data_use_measurement");
const char kImagePrefix[] = "/image";
}  // namespace

class PageLoadCappingBrowserTest : public InProcessBrowserTest {
 public:
  PageLoadCappingBrowserTest()
      : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {
    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    {
      scoped_refptr<base::FieldTrial> trial =
          base::FieldTrialList::CreateFieldTrial("TrialName1", "GroupName1");
      std::map<std::string, std::string> feature_parameters = {
          {"PageCapMiB", "0"}, {"PageFuzzingKiB", "0"}};
      base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
          "TrialName1", "GroupName1", feature_parameters);

      feature_list->RegisterFieldTrialOverride(
          data_use_measurement::page_load_capping::features::
              kDetectingHeavyPages.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    }

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Check if this matches the image requests from the test suite.
    if (!StartsWith(request.relative_url, kImagePrefix,
                    base::CompareCase::SENSITIVE)) {
      return nullptr;
    }
    // This request should match "/image.*" for this test suite.
    images_attempted_++;

    // Return a 404. This is expected in the test, but embedded test server will
    // create warnings when serving its own 404 responses.
    std::unique_ptr<net::test_server::BasicHttpResponse> not_found_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    not_found_response->set_code(net::HTTP_NOT_FOUND);
    if (waiting_) {
      run_loop_->QuitWhenIdle();
      waiting_ = false;
    }
    return not_found_response;
  }

  void PostToSelf() {
    EXPECT_FALSE(waiting_);
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  void WaitForRequest() {
    EXPECT_FALSE(waiting_);
    waiting_ = true;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  size_t images_attempted() const { return images_attempted_; }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;

 private:
  size_t images_attempted_ = 0u;
  bool waiting_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingBlocksLoads) {
  // Tests that subresource loading can be blocked from the browser process.
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server_->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));
  // Pause sub-resource loading.
  InfoBarService::FromWebContents(contents)
      ->infobar_at(0)
      ->delegate()
      ->AsConfirmInfoBarDelegate()
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // Adds images to the page. They should not be allowed to load.
  // Running this 20 times makes 20 round trips to the renderer, making it very
  // likely the earliest request would have made it to the network by the time
  // all of the calls have been made.
  for (size_t i = 0; i < 20; ++i) {
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = '")
            .append(kImagePrefix)
            .append(base::IntToString(i))
            .append(".png';");
    EXPECT_TRUE(content::ExecuteScript(contents, create_image_script));
  }

  // No images should be loaded as subresource loading was paused.
  EXPECT_EQ(0u, images_attempted());
  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlocksLoadsAndResume) {
  // Tests that after triggerring subresource pausing, resuming allows deferred
  // requests to be initiated.
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));

  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server_->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));
  // Pause sub-resource loading.
  InfoBarService::FromWebContents(contents)
      ->infobar_at(0)
      ->delegate()
      ->AsConfirmInfoBarDelegate()
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // Adds an image to the page. It should not be allowed to load at first.
  // PageLoadCappingBlocksLoads tests that it is not loaded more robustly
  std::string create_image_script =
      std::string(
          "var image = document.createElement('img'); "
          "document.body.appendChild(image); image.src = '")
          .append(kImagePrefix)
          .append(".png';");
  ASSERT_TRUE(content::ExecuteScript(contents, create_image_script));

  // Previous image should be allowed to load now.
  InfoBarService::FromWebContents(contents)
      ->infobar_at(0)
      ->delegate()
      ->AsConfirmInfoBarDelegate()
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // An image should be fetched because subresource loading was paused then
  // resumed.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest, PageLoadCappingAllowLoads) {
  // Tests that the image request loads normally when the page has not been
  // paused.
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(https_test_server_->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));

  // Adds an image to the page. It should be allowed to load.
  std::string create_image_script =
      std::string(
          "var image = document.createElement('img'); "
          "document.body.appendChild(image); image.src = '")
          .append(kImagePrefix)
          .append(".png';");
  ASSERT_TRUE(content::ExecuteScript(contents, create_image_script));

  // An image should be fetched because subresource loading was never paused.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());
  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlockNewFrameLoad) {
  // Tests that the image request loads normally when the page has not been
  // paused.
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(https_test_server_->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));
  // Pause sub-resource loading.
  InfoBarService::FromWebContents(contents)
      ->infobar_at(0)
      ->delegate()
      ->AsConfirmInfoBarDelegate()
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  content::TestNavigationObserver load_observer(contents);

  // Adds an image to the page. It should be allowed to load.
  std::string create_iframe_script = std::string(
      "var iframe = document.createElement('iframe');"
      "var html = '<body>NewFrame</body>';"
      "iframe.src = 'data:text/html;charset=utf-8,' + encodeURI(html);"
      "document.body.appendChild(iframe);");
  content::ExecuteScriptAsync(contents, create_iframe_script);

  // Make sure the DidFinishNavigation occured.
  load_observer.Wait();
  PostToSelf();

  size_t j = 0;
  for (auto* frame : contents->GetAllFrames()) {
    for (size_t i = 0; i < 20; ++i) {
      std::string create_image_script =
          std::string(
              "var image = document.createElement('img'); "
              "document.body.appendChild(image); image.src = '")
              .append(https_test_server_
                          ->GetURL(std::string(kImagePrefix)
                                       .append(base::IntToString(++j))
                                       .append(".png';"))
                          .spec());

      EXPECT_TRUE(content::ExecuteScript(frame, create_image_script));
    }
  }

  // An image should not be fetched because subresource loading was paused in
  // both frames.
  EXPECT_EQ(0u, images_attempted());
  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingBlockNewFrameLoadResume) {
  // Tests that the image request loads normally when the page has not been
  // paused.
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(https_test_server_->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Load a mostly empty page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));
  // Pause sub-resource loading.
  InfoBarService::FromWebContents(contents)
      ->infobar_at(0)
      ->delegate()
      ->AsConfirmInfoBarDelegate()
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);
  content::TestNavigationObserver load_observer(contents);

  // Adds an image to the page. It should be allowed to load.
  std::string create_iframe_script = std::string(
      "var iframe = document.createElement('iframe');"
      "var html = '<body>NewFrame</body>';"
      "iframe.src = 'data:text/html;charset=utf-8,' + encodeURI(html);"
      "document.body.appendChild(iframe);");
  content::ExecuteScriptAsync(contents, create_iframe_script);

  // Make sure the DidFinishNavigation occured.
  load_observer.Wait();
  PostToSelf();

  for (auto* frame : contents->GetAllFrames()) {
    if (contents->GetMainFrame() == frame)
      continue;
    std::string create_image_script =
        std::string(
            "var image = document.createElement('img'); "
            "document.body.appendChild(image); image.src = '")
            .append(https_test_server_
                        ->GetURL(std::string(kImagePrefix).append(".png';"))
                        .spec());
    ASSERT_TRUE(content::ExecuteScript(frame, create_image_script));
  }

  // An image should not be fetched because subresource loading was paused in
  // both frames.
  EXPECT_EQ(0u, images_attempted());

  // Previous image should be allowed to load now.
  InfoBarService::FromWebContents(contents)
      ->infobar_at(0)
      ->delegate()
      ->AsConfirmInfoBarDelegate()
      ->LinkClicked(WindowOpenDisposition::CURRENT_TAB);

  // An image should be fetched because subresource loading was resumed.
  if (images_attempted() < 1u)
    WaitForRequest();
  EXPECT_EQ(1u, images_attempted());

  https_test_server_.reset();
}

IN_PROC_BROWSER_TEST_F(PageLoadCappingBrowserTest,
                       PageLoadCappingInfobarShownAfterSamePageNavigation) {
  https_test_server_->RegisterRequestHandler(base::BindRepeating(
      &PageLoadCappingBrowserTest::HandleRequest, base::Unretained(this)));
  https_test_server_->ServeFilesFromSourceDirectory(base::FilePath(kDocRoot));

  ASSERT_TRUE(https_test_server_->Start());

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  // Load a page.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html"));

  ASSERT_EQ(1u, InfoBarService::FromWebContents(contents)->infobar_count());
  infobars::InfoBar* infobar =
      InfoBarService::FromWebContents(contents)->infobar_at(0);

  // Navigate on the page to an anchor.
  ui_test_utils::NavigateToURL(
      browser(), https_test_server_->GetURL("/page_capping.html#anchor"));

  EXPECT_EQ(1u, InfoBarService::FromWebContents(contents)->infobar_count());
  EXPECT_EQ(infobar, InfoBarService::FromWebContents(contents)->infobar_at(0));

  https_test_server_.reset();
}
