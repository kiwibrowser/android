// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_GBM_DEVICE_H_
#define UI_OZONE_PLATFORM_DRM_GPU_GBM_DEVICE_H_

#include "base/macros.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"

namespace ui {

class GbmDevice : public DrmDevice {
 public:
  GbmDevice(const base::FilePath& device_path,
            base::File file,
            bool is_primary_device);

  // DrmDevice implementation:
  bool Initialize() override;

 private:
  ~GbmDevice() override;

  DISALLOW_COPY_AND_ASSIGN(GbmDevice);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_GBM_DEVICE_H_
