// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

using content::RenderFrameHost;
using testing::CreateSuffixRule;

class AdTaggingBrowserTest : public SubresourceFilterBrowserTest {
 public:
  AdTaggingBrowserTest() : SubresourceFilterBrowserTest() {}
  ~AdTaggingBrowserTest() override {}

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {CreateSuffixRule("ad_script.js"), CreateSuffixRule("ad=true")});
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Used for giving identifiers to frames that can easily be searched for
  // with content::FrameMatchingPredicate.
  std::string GetUniqueFrameName() {
    return base::StringPrintf("frame_%d", frame_count_++);
  }

  // Create a frame that navigates via the src attribute. It's created by ad
  // script. Returns after navigation has completed.
  content::RenderFrameHost* CreateSrcFrameFromAdScript(
      const content::ToRenderFrameHost& adapter,
      const GURL& url) {
    return CreateFrameImpl(adapter, url, true /* ad_script */);
  }

  // Create a frame that navigates via the src attribute. Returns after
  // navigation has completed.
  content::RenderFrameHost* CreateSrcFrame(
      const content::ToRenderFrameHost& adapter,
      const GURL& url) {
    return CreateFrameImpl(adapter, url, false /* ad_script */);
  }

  // Creates a frame and doc.writes the frame into it. Returns after
  // navigation has completed.
  content::RenderFrameHost* CreateDocWrittenFrame(
      const content::ToRenderFrameHost& adapter) {
    return CreateDocWrittenFrameImpl(adapter, false /* ad_script */);
  }

  // Creates a frame and doc.writes the frame into it. The script creating the
  // frame is an ad script. Returns after navigation has completed.
  content::RenderFrameHost* CreateDocWrittenFrameFromAdScript(
      const content::ToRenderFrameHost& adapter) {
    return CreateDocWrittenFrameImpl(adapter, true /* ad_script */);
  }

  // Given a RenderFrameHost, navigates the page to the given |url| and waits
  // for the navigation to complete before returning.
  void NavigateFrame(content::RenderFrameHost* render_frame_host,
                     const GURL& url);

  GURL GetURL(const std::string& page) {
    return embedded_test_server()->GetURL("/ad_tagging/" + page);
  }

 private:
  content::RenderFrameHost* CreateFrameImpl(
      const content::ToRenderFrameHost& adapter,
      const GURL& url,
      bool ad_script);

  content::RenderFrameHost* CreateDocWrittenFrameImpl(
      const content::ToRenderFrameHost& adapter,
      bool ad_script);

  uint32_t frame_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AdTaggingBrowserTest);
};

content::RenderFrameHost* AdTaggingBrowserTest::CreateFrameImpl(
    const content::ToRenderFrameHost& adapter,
    const GURL& url,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();
  std::string script = base::StringPrintf(
      "%s('%s','%s');", ad_script ? "createAdFrame" : "createFrame",
      url.spec().c_str(), name.c_str());

  content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
  EXPECT_TRUE(content::ExecuteScript(rfh, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
  return content::FrameMatchingPredicate(
      GetWebContents(), base::BindRepeating(&content::FrameMatchesName, name));
}

content::RenderFrameHost* AdTaggingBrowserTest::CreateDocWrittenFrameImpl(
    const content::ToRenderFrameHost& adapter,
    bool ad_script) {
  content::RenderFrameHost* rfh = adapter.render_frame_host();
  std::string name = GetUniqueFrameName();

  std::string script = base::StringPrintf(
      "%s('%s', '%s');",
      ad_script ? "createDocWrittenAdFrame" : "createDocWrittenFrame",
      name.c_str(), GetURL("").spec().c_str());

  content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
  EXPECT_TRUE(content::ExecuteScript(rfh, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
  return content::FrameMatchingPredicate(
      GetWebContents(), base::BindRepeating(&content::FrameMatchesName, name));
}

// Given a RenderFrameHost, navigates the page to the given |url| and waits
// for the navigation to complete before returning.
void AdTaggingBrowserTest::NavigateFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url) {
  std::string script =
      base::StringPrintf(R"(window.location='%s')", url.spec().c_str());
  content::TestNavigationObserver navigation_observer(GetWebContents(), 1);
  EXPECT_TRUE(content::ExecuteScript(render_frame_host, script));
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded())
      << navigation_observer.last_net_error_code();
}

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FramesByURL) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));
  EXPECT_FALSE(
      observer.GetIsAdSubframe(GetURL("frame_factory.html")).has_value());

  // (1) Vanilla child.
  RenderFrameHost* vanilla_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_FALSE(*observer.GetIsAdSubframe(GetURL("frame_factory.html?1")));

  // (2) Ad child.
  RenderFrameHost* ad_child =
      CreateSrcFrame(GetWebContents(), GetURL("frame_factory.html?2&ad=true"));
  EXPECT_TRUE(
      *observer.GetIsAdSubframe(GetURL("frame_factory.html?2&ad=true")));

  // (3) Ad child of 2.
  CreateSrcFrame(ad_child, GetURL("frame_factory.html?sub=1&3&ad=true"));
  EXPECT_TRUE(
      *observer.GetIsAdSubframe(GetURL("frame_factory.html?sub=1&3&ad=true")));

  // (4) Vanilla child of 2.
  CreateSrcFrame(ad_child, GetURL("frame_factory.html?4"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(GetURL("frame_factory.html?4")));

  // (5) Vanilla child of 1. This tests something subtle.
  // frame_factory.html?ad=true loads the same script that frame_factory.html
  // uses to load frames. This tests that even though the script is tagged as an
  // ad in the ad iframe, it's not considered an ad in the main frame, hence
  // it's able to create an iframe that's not labeled as an ad.
  CreateSrcFrame(vanilla_child, GetURL("frame_factory.html?5"));
  EXPECT_FALSE(*observer.GetIsAdSubframe(GetURL("frame_factory.html?5")));
}

// Test that a subframe with a non-ad url but loaded by ad script is an ad
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, FrameLoadedByAdScript) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // Child frame created by ad script.
  CreateSrcFrameFromAdScript(GetWebContents(), GetURL("frame_factory.html?1"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(GetURL("frame_factory.html?1")));
}

// Test that same-origin doc.write created iframes are tagged as ads.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, SameOriginFrameTagging) {
  TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  ui_test_utils::NavigateToURL(browser(), GetURL("frame_factory.html"));

  // (1) Vanilla child.
  content::RenderFrameHost* vanilla_frame =
      CreateDocWrittenFrame(GetWebContents());

  // Navigate the child to a vanilla site to trigger an observer event. If the
  // first navigation was considered an ad, the second should be as well.
  // TOOD(jkarlin): The extra navigations in this test aren't necessary once
  // https://crbug.com/849268 is fixed.
  NavigateFrame(vanilla_frame, GetURL("frame_factory.html?1"));
  EXPECT_FALSE(*observer.GetIsAdSubframe(GetURL("frame_factory.html?1")));

  // (2) Ad child.
  content::RenderFrameHost* ad_frame =
      CreateDocWrittenFrameFromAdScript(GetWebContents());
  NavigateFrame(ad_frame, GetURL("frame_factory.html?2"));
  EXPECT_TRUE(*observer.GetIsAdSubframe(GetURL("frame_factory.html?2")));
}

}  // namespace

}  // namespace subresource_filter
