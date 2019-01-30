// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_CREATE_DESKTOP_MEDIA_PICKER_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_CREATE_DESKTOP_MEDIA_PICKER_COCOA_H_

#include <memory>

#include "chrome/browser/media/webrtc/desktop_media_picker.h"

// Returns a Cocoa DesktopMediaPicker or nullptr if the Views version should be
// used instead.
std::unique_ptr<DesktopMediaPicker> CreateDesktopMediaPickerCocoa();

#endif  // CHROME_BROWSER_UI_COCOA_MEDIA_PICKER_CREATE_DESKTOP_MEDIA_PICKER_COCOA_H_
