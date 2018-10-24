// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/picture_in_picture_window_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/dns/mock_host_resolver.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#endif

class PictureInPictureWindowControllerBrowserTest
    : public InProcessBrowserTest {
 public:
  PictureInPictureWindowControllerBrowserTest() {
    feature_list_.InitWithFeatures(
        {media::kPictureInPicture, media::kUseSurfaceLayerForVideo}, {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpWindowController(content::WebContents* web_contents) {
    pip_window_controller_ =
        content::PictureInPictureWindowController::GetOrCreateForWebContents(
            web_contents);
  }

  content::PictureInPictureWindowController* window_controller() {
    return pip_window_controller_;
  }

 private:
  content::PictureInPictureWindowController* pip_window_controller_ = nullptr;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowControllerBrowserTest);
};

// Checks the creation of the window controller, as well as basic window
// creation and visibility.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CreationAndVisibility) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller() != nullptr);

  ASSERT_TRUE(window_controller()->GetWindowForTesting() != nullptr);
  ASSERT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Wait for resize event from the page.
  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Tests that when an active WebContents accurately tracks whether a video
// is in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       TabIconUpdated) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  // First test there is no video playing in Picture-in-Picture.
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());

  // Start playing video in Picture-in-Picture and retest with the
  // opposite assertion.
  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Wait for title update to confirm and then test there is video playing in
  // Picture-in-Picture.
  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(active_web_contents->HasPictureInPictureVideo());

  // Stop video being played Picture-in-Picture and check if that's tracked.
  window_controller()->Close(true /* should_pause_video */);
  EXPECT_FALSE(active_web_contents->HasPictureInPictureVideo());
}

#if !defined(OS_ANDROID)

// Tests that when creating a Picture-in-Picture window a size is sent to the
// caller and if the window is resized, the caller is also notified.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       ResizeEventFired) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  content::OverlayWindow* overlay_window =
      window_controller()->GetWindowForTesting();
  ASSERT_TRUE(overlay_window);
  ASSERT_FALSE(overlay_window->IsVisible());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  static_cast<OverlayWindowViews*>(overlay_window)
      ->SetSize(gfx::Size(400, 400));

  expected_title = base::ASCIIToUTF16("2");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

#endif  // !defined(OS_ANDROID)

// Tests that when closing a Picture-in-Picture window, the video element is
// reflected as no longer in Picture-in-Picture.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  window_controller()->Close(true /* should_pause_video */);

  expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_TRUE(is_paused);
}

// Ditto, when the video isn't playing.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowWithoutPlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  window_controller()->Close(true /* should_pause_video */);

  expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that when closing a Picture-in-Picture window from the Web API, the
// video element is not paused.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseWindowFromWebAPIWhilePlaying) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "exitPictureInPicture();"));

  // 'left' is sent when the first video leaves Picture-in-Picture.
  expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);
}

// Tests that when starting a new Picture-in-Picture session from the same
// video, the video stays in Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureTwiceFromSameVideo) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Check that requesting Picture-in-Picture promise was resolved.
  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "exitPictureInPicture();"));

  // 'left' is sent when the video leaves Picture-in-Picture.
  expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Check that requesting Picture-in-Picture promise was resolved.
  expected_title = base::ASCIIToUTF16("2");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);
}

// Tests that when starting a new Picture-in-Picture session from the same tab,
// the previous video is no longer in Picture-in-Picture mode.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       OpenSecondPictureInPictureStopsFirst) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Check that the Picture-in-Picture window was resized once.
  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_TRUE(in_picture_in_picture);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "secondPictureInPicture();"));

  // 'left' is sent when the first video leaves Picture-in-Picture.
  expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  in_picture_in_picture = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      active_web_contents, "isInPictureInPicture();", &in_picture_in_picture));
  EXPECT_FALSE(in_picture_in_picture);

  bool is_paused = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(active_web_contents, "isPaused();",
                                          &is_paused));
  EXPECT_FALSE(is_paused);
}

// Tests that we can enter Picture-in-Picture when a video is not preloaded,
// using the metadata optimizations. This test is checking that there are no
// crashes.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       EnterMetadataPosterOptimisation) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL(
          "media/picture-in-picture/player_metadata_poster.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  base::string16 expected_title = base::ASCIIToUTF16("entered_pip");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());
}

// Tests that calling PictureInPictureWindowController::Close() twice has no
// side effect.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       CloseTwiceSideEffects) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents);

  SetUpWindowController(active_web_contents);
  ASSERT_TRUE(window_controller());

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Wait for resize event from the page.
  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  window_controller()->Close(true /* should_pause_video */);

  // Wait for the window to close.
  expected_title = base::ASCIIToUTF16("left");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  bool video_paused = false;

  // Video is paused after Picture-in-Picture window was closed.
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isPaused();", &video_paused));
  EXPECT_TRUE(video_paused);

  // Resume playback.
  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "video.play();"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isPaused();", &video_paused));
  EXPECT_FALSE(video_paused);

  // This should be a no-op because the window is not visible.
  window_controller()->Close(true /* should_pause_video */);

  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      active_web_contents, "isPaused();", &video_paused));
  EXPECT_FALSE(video_paused);
}

// Checks entering Picture-in-Picture on multiple tabs, where the initial tab
// has been closed.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       PictureInPictureAfterClosingTab) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Wait for resize event from the page.
  base::string16 expected_title = base::ASCIIToUTF16("1");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  // Open a new tab in the browser.
  AddTabAtIndex(1, test_page_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Once the initiator tab is closed, the controller should also be torn down.
  browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Open video in Picture-in-Picture mode again, on the new tab.
  active_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  EXPECT_TRUE(
      content::ExecuteScript(active_web_contents, "enterPictureInPicture();"));

  // Wait for resize event from the page.
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  ASSERT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Checks setting disablePictureInPicture on video just after requesting
// Picture-in-Picture doesn't result in a window opened.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       RequestPictureInPictureAfterDisablePictureInPicture) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/window-size.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  EXPECT_TRUE(content::ExecuteScript(active_web_contents,
                                     "requestPictureInPictureAndDisable();"));

  // Wait for promise to reject.
  base::string16 expected_title = base::ASCIIToUTF16("rejected");
  EXPECT_EQ(expected_title,
            content::TitleWatcher(active_web_contents, expected_title)
                .WaitAndGetTitle());

  ASSERT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Checks that a video in Picture-in-Picture stops if its iframe is removed.
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       FrameEnterLeaveClosesWindow) {
  GURL test_page_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(
          FILE_PATH_LITERAL("media/picture-in-picture/iframe-test.html")));
  ui_test_utils::NavigateToURL(browser(), test_page_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      active_web_contents->GetAllFrames();
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      iframe, "enterPictureInPicture();", &result));
  EXPECT_EQ("done", result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, active_web_contents->GetAllFrames().size());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}

// Same as above for a cross-origin iframe.
// Flaky on windows and Linux: crbug/854349
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_CrossOriginFrameEnterLeaveCloseWindow \
  DISABLED_CrossOriginFrameEnterLeaveCloseWindow
#else
#define MAYBE_CrossOriginFrameEnterLeaveCloseWindow \
  CrossOriginFrameEnterLeaveCloseWindow
#endif
IN_PROC_BROWSER_TEST_F(PictureInPictureWindowControllerBrowserTest,
                       MAYBE_CrossOriginFrameEnterLeaveCloseWindow) {
  GURL embed_url = embedded_test_server()->GetURL(
      "a.com", "/media/picture-in-picture/iframe-content.html");
  GURL main_url = embedded_test_server()->GetURL(
      "example.com", "/media/picture-in-picture/iframe-test.html?embed_url=" +
                         embed_url.spec());

  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_web_contents != nullptr);

  SetUpWindowController(active_web_contents);

  std::vector<content::RenderFrameHost*> render_frame_hosts =
      active_web_contents->GetAllFrames();
  ASSERT_EQ(2u, render_frame_hosts.size());

  content::RenderFrameHost* iframe =
      render_frame_hosts[0] == active_web_contents->GetMainFrame()
          ? render_frame_hosts[1]
          : render_frame_hosts[0];

  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      iframe, "enterPictureInPicture();", &result));
  EXPECT_EQ("done", result);

  EXPECT_TRUE(window_controller()->GetWindowForTesting()->IsVisible());

  ASSERT_TRUE(content::ExecuteScript(active_web_contents, "removeFrame();"));

  EXPECT_EQ(1u, active_web_contents->GetAllFrames().size());
  EXPECT_FALSE(window_controller()->GetWindowForTesting()->IsVisible());
}
