// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojo/clipboard_client.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace ui {

namespace {

std::unique_ptr<views::ScopedViewsTestHelper> g_scoped_views_test_helper;

}  // namespace

struct PlatformClipboardTraits {
  static std::unique_ptr<PlatformEventSource> GetEventSource() {
    return nullptr;
  }

  static Clipboard* Create() {
    g_scoped_views_test_helper =
        std::make_unique<views::ScopedViewsTestHelper>();
    EXPECT_TRUE(views::MusClient::Exists());
    return Clipboard::GetForCurrentThread();
  }

  static void Destroy(Clipboard* clipboard) {
    g_scoped_views_test_helper.reset();
  }
};

using TypesToTest = PlatformClipboardTraits;

}  // namespace ui

#include "ui/base/clipboard/clipboard_test_template.h"
