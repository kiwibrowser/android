// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_SCANOUT_BUFFER_GENERATOR_H_
#define UI_OZONE_PLATFORM_DRM_GPU_SCANOUT_BUFFER_GENERATOR_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

class DrmDevice;
class ScanoutBuffer;

class ScanoutBufferGenerator {
 public:
  virtual ~ScanoutBufferGenerator() {}

  virtual scoped_refptr<ScanoutBuffer> Create(
      const scoped_refptr<DrmDevice>& drm,
      uint32_t format,
      const std::vector<uint64_t>& modifiers,
      const gfx::Size& size) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_SCANOUT_BUFFER_GENERATOR_H_
