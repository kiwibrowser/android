// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/test_shell_main_delegate.h"

#include "content/shell/browser/shell_application_mac.h"

namespace extensions {

void TestShellMainDelegate::PreContentInitialization() {
  // Force the NSApplication subclass to be used.
  [ShellCrApplication sharedApplication];
}

}  // namespace extensions
