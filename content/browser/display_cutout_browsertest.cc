// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"

namespace content {

namespace {

class TestWebContentsObserver : public WebContentsObserver {
 public:
  explicit TestWebContentsObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver override.
  void ViewportFitChanged(blink::mojom::ViewportFit value) override {
    value_ = value;

    if (value_ == wanted_value_)
      run_loop_.Quit();
  }

  void WaitForWantedValue(blink::mojom::ViewportFit wanted_value) {
    if (value_.has_value() && value_ == wanted_value)
      return;

    wanted_value_ = wanted_value;
    run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  base::Optional<blink::mojom::ViewportFit> value_;
  blink::mojom::ViewportFit wanted_value_ = blink::mojom::ViewportFit::kAuto;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsObserver);
};

}  // namespace

class DisplayCutoutBrowserTest : public ContentBrowserTest {
 public:
  DisplayCutoutBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("enable-blink-features",
                                    "DisplayCutoutAPI,CSSViewport");
  }

  void LoadTestPageWithViewportFitFromMeta(const std::string& value) {
    LoadTestPageWithData(
        "<!DOCTYPE html>"
        "<meta name='viewport' content='viewport-fit=" +
        value + "'><iframe></iframe>");
  }

  void LoadTestPageWithViewportFitFromCSS(const std::string& value) {
    LoadTestPageWithData(
        "<!DOCTYPE html><head>"
        "<style>@viewport { viewport-fit: " +
        value +
        "; }</style>"
        "<iframe></iframe>");
  }

  void LoadSubFrameWithViewportFitMetaValue(const std::string& value) {
    LoadSubFrameWithData(
        "<meta name='viewport' content='viewport-fit=" + value + "'>");
  }

  void LoadTestPageWithNoViewportFit() {
    LoadTestPageWithData("<!DOCTYPE html>");
  }

  void LoadTestPageWithDifferentOrigin() {
    GURL url("https://www.example.org");
    content::LoadDataWithBaseURL(shell(), url, "<!DOCTYPE html>", url);
  }

  bool ClearViewportFitTag() {
    return ExecuteScript(
        shell()->web_contents(),
        "document.getElementsByTagName('meta')[0].content = ''");
  }

 private:
  void LoadSubFrameWithData(const std::string& html_data) {
    const std::string data =
        "data:text/html;charset=utf-8,"
        "<!DOCTYPE html>" +
        html_data;

    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    FrameTreeNode* root = web_contents->GetFrameTree()->root();

    NavigationController::LoadURLParams params(GURL::EmptyGURL());
    params.url = GURL(data);
    params.frame_tree_node_id = root->child_at(0)->frame_tree_node_id();
    params.load_type = NavigationController::LOAD_TYPE_DATA;
    web_contents->GetController().LoadURLWithParams(params);
    web_contents->Focus();
  }

  void LoadTestPageWithData(const std::string& data) {
    GURL url("https://www.example.com");

    TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
#if defined(OS_ANDROID)
    shell()->LoadDataAsStringWithBaseURL(url, data, url);
#else
    shell()->LoadDataWithBaseURL(url, data, url);
#endif
    same_tab_observer.Wait();
  }

  DISALLOW_COPY_AND_ASSIGN(DisplayCutoutBrowserTest);
};

// The viewport meta tag is only enabled on Android.
#if defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_Auto) {
  // Since kAuto is the default we need to load a page first to force the
  // WebContents to fire an event when we change it.
  LoadTestPageWithViewportFitFromMeta("cover");

  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromMeta("auto");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_Contain) {
  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromMeta("contain");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kContain);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_Cover) {
  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromMeta("cover");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_Default) {
  // Since kAuto is the default we need to load a page first to force the
  // WebContents to fire an event when we change it.
  LoadTestPageWithViewportFitFromMeta("cover");

  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithNoViewportFit();
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_Invalid) {
  // Since kAuto is the default we need to load a page first to force the
  // WebContents to fire an event when we change it.
  LoadTestPageWithViewportFitFromMeta("cover");

  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromMeta("invalid");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_Update) {
  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromMeta("cover");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);

  EXPECT_TRUE(ClearViewportFitTag());
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_Meta_SubFrame) {
  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromMeta("contain");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kContain);

  LoadSubFrameWithViewportFitMetaValue("cover");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);
}

#endif

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_CSS_Auto) {
  // Since kAuto is the default we need to load a page first to force the
  // WebContents to fire an event when we change it.
  LoadTestPageWithViewportFitFromCSS("cover");

  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromCSS("auto");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_CSS_Contain) {
  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromCSS("contain");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kContain);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_CSS_Cover) {
  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromCSS("cover");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kCover);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_CSS_Default) {
  // Since kAuto is the default we need to load a page first to force the
  // WebContents to fire an event when we change it.
  LoadTestPageWithViewportFitFromCSS("cover");

  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithNoViewportFit();
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

IN_PROC_BROWSER_TEST_F(DisplayCutoutBrowserTest, ViewportFit_CSS_Invalid) {
  // Since kAuto is the default we need to load a page first to force the
  // WebContents to fire an event when we change it.
  LoadTestPageWithViewportFitFromCSS("cover");

  TestWebContentsObserver observer(shell()->web_contents());
  LoadTestPageWithViewportFitFromCSS("invalid");
  observer.WaitForWantedValue(blink::mojom::ViewportFit::kAuto);
}

}  //  namespace content
