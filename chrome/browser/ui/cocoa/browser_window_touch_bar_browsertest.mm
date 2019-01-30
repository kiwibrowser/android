// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_touch_bar.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/scoped_macviews_browser_mode.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_test_util.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest_mac.h"

// TODO(spqchan): Write tests that will check for page load and bookmark
// updates.

class BrowserWindowTouchBarTest : public InProcessBrowserTest {
 public:
  BrowserWindowTouchBarTest() : InProcessBrowserTest() {}

  void SetUpOnMainThread() override {
    browser_touch_bar_.reset([[BrowserWindowTouchBar alloc]
        initWithBrowser:browser()
                 window:[browser_window_controller() window]]);
    [browser_window_controller()
        setBrowserWindowTouchBar:browser_touch_bar_.get()];
  }

  void TearDownOnMainThread() override {
    DestroyBrowserWindowTouchBar();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void DestroyBrowserWindowTouchBar() {
    [browser_window_controller() setBrowserWindowTouchBar:nil];
    browser_touch_bar_.reset();
  }

  BrowserWindowController* browser_window_controller() {
    return [BrowserWindowController
        browserWindowControllerForWindow:browser()
                                             ->window()
                                             ->GetNativeWindow()];
  }

  BrowserWindowTouchBar* browser_touch_bar() const {
    return browser_touch_bar_;
  }

 private:
  base::scoped_nsobject<BrowserWindowTouchBar> browser_touch_bar_;

  test::ScopedMacViewsBrowserMode cocoa_browser_mode_{false};

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowTouchBarTest);
};

// Test if the touch bar gets invalidated when the active tab is changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarTest, TabChanges) {
  if (@available(macOS 10.12.2, *)) {
    NSWindow* window = [browser_window_controller() window];
    NSTouchBar* touch_bar = [browser_touch_bar() makeTouchBar];
    [window setTouchBar:touch_bar];
    EXPECT_TRUE([window touchBar]);

    // Insert a new tab in the foreground. The window should have a new touch
    // bar.
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    EXPECT_NE(touch_bar, [window touchBar]);
  }
}

// Tests if the touch bar gets invalidated if the default search engine has
// changed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarTest, SearchEngineChanges) {
  if (@available(macOS 10.12.2, *)) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    DCHECK(prefs);

    NSWindow* window = [browser_window_controller() window];
    NSTouchBar* touch_bar = [browser_touch_bar() makeTouchBar];
    [window setTouchBar:touch_bar];
    EXPECT_TRUE([window touchBar]);

    // Change the default search engine.
    std::unique_ptr<TemplateURLData> data =
        GenerateDummyTemplateURLData("poutine");
    prefs->Set(DefaultSearchManager::kDefaultSearchProviderDataPrefName,
               *TemplateURLDataToDictionary(*data));

    // The window should have a new touch bar.
    EXPECT_NE(touch_bar, [window touchBar]);
  }
}

// Tests to see if the touch bar's bookmark tab helper observer gets removed
// when the touch bar is destroyed.
IN_PROC_BROWSER_TEST_F(BrowserWindowTouchBarTest, DestroyNotificationBridge) {
  if (@available(macOS 10.12.2, *)) {
    BookmarkTabHelperObserver* observer =
        [browser_touch_bar() bookmarkTabObserver];
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);

    BookmarkTabHelper* tab_helper = BookmarkTabHelper::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    DCHECK(tab_helper);
    EXPECT_TRUE(tab_helper->HasObserver(observer));

    DestroyBrowserWindowTouchBar();
    EXPECT_FALSE(tab_helper->HasObserver(observer));
  }
}
