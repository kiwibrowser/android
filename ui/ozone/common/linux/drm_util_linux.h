// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_DRM_UTIL_LINUX_H_
#define UI_OZONE_COMMON_LINUX_DRM_UTIL_LINUX_H_

#include "base/files/file_path.h"
#include "ui/gfx/buffer_types.h"

namespace ui {

int GetFourCCFormatFromBufferFormat(gfx::BufferFormat format);
gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format);

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_DRM_UTIL_LINUX_H__
