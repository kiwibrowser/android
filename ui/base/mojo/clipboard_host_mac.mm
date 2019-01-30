// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojo/clipboard_host.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/strings/sys_string_conversions.h"
#import "ui/base/cocoa/find_pasteboard.h"

namespace ui {
namespace {

// The number of utf16 code units that will be written to the find pasteboard,
// longer texts are silently ignored. This is to prevent that a compromised
// renderer can write unlimited amounts of data into the find pasteboard.
static constexpr size_t kMaxFindPboardStringLength = 4096;

}  // namespace

void ClipboardHost::WriteStringToFindPboard(const base::string16& text) {
  if (text.length() <= kMaxFindPboardStringLength) {
    NSString* nsText = base::SysUTF16ToNSString(text);
    if (nsText) {
      [[FindPasteboard sharedInstance] setFindText:nsText];
    }
  }
}

}  // namespace ui
