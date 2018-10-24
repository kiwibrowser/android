// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/media_picker/create_desktop_media_picker_cocoa.h"

#import "chrome/browser/ui/cocoa/media_picker/desktop_media_picker_cocoa.h"

// static
std::unique_ptr<DesktopMediaPicker> CreateDesktopMediaPickerCocoa() {
  return std::make_unique<DesktopMediaPickerCocoa>();
}
