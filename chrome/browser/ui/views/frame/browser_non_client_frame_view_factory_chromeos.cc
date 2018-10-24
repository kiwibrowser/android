// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_mash.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/ui_base_features.h"

namespace chrome {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view) {
  if (!features::IsAshInBrowserProcess()) {
    BrowserNonClientFrameViewMash* frame_view =
        new BrowserNonClientFrameViewMash(frame, browser_view);
    frame_view->Init();
    return frame_view;
  }
  BrowserNonClientFrameViewAsh* frame_view =
      new BrowserNonClientFrameViewAsh(frame, browser_view);
  frame_view->Init();
  return frame_view;
}

}  // namespace chrome
