// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_DUMMY_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_DUMMY_H_

#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

namespace ui {

// Fake plane used with DRM legacy when universal planes are not supported and
// the kernel doesn't report the primary plane.
class HardwareDisplayPlaneDummy : public HardwareDisplayPlane {
 public:
  HardwareDisplayPlaneDummy(uint32_t id, uint32_t crtc_mask);
  ~HardwareDisplayPlaneDummy() override;

  bool Initialize(DrmDevice* drm) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneDummy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_DUMMY_H_
