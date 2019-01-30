// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"
#include "chrome/browser/ui/views/frame/browser_frame_mash.h"
#include "ui/base/ui_base_features.h"

NativeBrowserFrame* NativeBrowserFrameFactory::Create(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  if (!features::IsAshInBrowserProcess())
    return new BrowserFrameMash(browser_frame, browser_view);
  return new BrowserFrameAsh(browser_frame, browser_view);
}
