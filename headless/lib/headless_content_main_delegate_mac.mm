// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/headless_content_main_delegate.h"

#include "headless/lib/browser/headless_shell_application_mac.h"

namespace headless {

void HeadlessContentMainDelegate::PreContentInitialization() {
  // Force the NSApplication subclass to be used.
  [HeadlessShellCrApplication sharedApplication];
}

}  // namespace headless
