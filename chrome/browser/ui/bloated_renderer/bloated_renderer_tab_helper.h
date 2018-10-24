// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOATED_RENDERER_BLOATED_RENDERER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOATED_RENDERER_BLOATED_RENDERER_TAB_HELPER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarService;

// This tab helper observes the WillReloadBloatedRenderer event. Upon
// receiving the event, it activates the logic to show an infobar on the
// subsequent DidFinishNavigation event.
//
// Assumptions around the WillReloadBloatedRenderer event:
// - The renderer process was shutdown before it.
// - Page reload will be performed immediately after it.
// This ensures that the first DidFinishNavigation after it originates from
// reloading the bloated page.
//
// Note that we need to show the infobar after NavigationEntryCommitted
// because the infobar service removes existing infobars there.
class BloatedRendererTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BloatedRendererTabHelper> {
 public:
  ~BloatedRendererTabHelper() override = default;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void WillReloadPageWithBloatedRenderer();
  static void ShowInfoBar(InfoBarService* infobar_service);

 private:
  friend class content::WebContentsUserData<BloatedRendererTabHelper>;
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest, DetectReload);

  explicit BloatedRendererTabHelper(content::WebContents* contents);

  bool reloading_bloated_renderer_ = false;

  DISALLOW_COPY_AND_ASSIGN(BloatedRendererTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOATED_RENDERER_BLOATED_RENDERER_TAB_HELPER_H_
