// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_dummy.h"

#include <drm_fourcc.h>

namespace ui {

HardwareDisplayPlaneDummy::HardwareDisplayPlaneDummy(uint32_t id,
                                                     uint32_t crtc_mask)
    : HardwareDisplayPlane(id) {
  crtc_mask_ = crtc_mask;
}

HardwareDisplayPlaneDummy::~HardwareDisplayPlaneDummy() {}

bool HardwareDisplayPlaneDummy::Initialize(DrmDevice* drm) {
  type_ = kDummy;
  supported_formats_.push_back(DRM_FORMAT_XRGB8888);
  supported_formats_.push_back(DRM_FORMAT_XBGR8888);
  return true;
}

}  // namespace ui
