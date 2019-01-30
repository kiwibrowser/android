// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/test/test_clipboard.h"
#include "ui/gfx/skia_util.h"

namespace content {

class ClipboardHostImplTest : public ::testing::Test {
 protected:
  ClipboardHostImplTest()
      : clipboard_(ui::TestClipboard::CreateForCurrentThread()) {}

  ~ClipboardHostImplTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  void CallWriteImage(const SkBitmap& bitmap) {
    host_.WriteImage(ui::CLIPBOARD_TYPE_COPY_PASTE, bitmap);
  }

  void CallCommitWrite() {
    host_.CommitWrite(ui::CLIPBOARD_TYPE_COPY_PASTE);
    base::RunLoop().RunUntilIdle();
  }

  ui::Clipboard* clipboard() { return clipboard_; }

 private:
  const TestBrowserThreadBundle thread_bundle_;
  ClipboardHostImpl host_;
  ui::Clipboard* const clipboard_;
};

// Test that it actually works.
TEST_F(ClipboardHostImplTest, SimpleImage) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(3, 2);
  bitmap.eraseARGB(255, 0, 255, 0);
  CallWriteImage(bitmap);
  uint64_t sequence_number =
      clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE);
  CallCommitWrite();

  EXPECT_NE(sequence_number,
            clipboard()->GetSequenceNumber(ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_FALSE(clipboard()->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::CLIPBOARD_TYPE_COPY_PASTE));
  EXPECT_TRUE(clipboard()->IsFormatAvailable(
      ui::Clipboard::GetBitmapFormatType(), ui::CLIPBOARD_TYPE_COPY_PASTE));

  SkBitmap actual = clipboard()->ReadImage(ui::CLIPBOARD_TYPE_COPY_PASTE);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, actual));
}

}  // namespace content
