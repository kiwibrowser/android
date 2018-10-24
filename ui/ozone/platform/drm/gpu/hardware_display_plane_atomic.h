// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_
#define UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_

#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

#include <stdint.h>
#include <xf86drmMode.h>

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class CrtcController;

class HardwareDisplayPlaneAtomic : public HardwareDisplayPlane {
 public:
  HardwareDisplayPlaneAtomic(uint32_t id);
  ~HardwareDisplayPlaneAtomic() override;

  bool Initialize(DrmDevice* drm) override;

  virtual bool SetPlaneData(drmModeAtomicReq* property_set,
                            uint32_t crtc_id,
                            uint32_t framebuffer,
                            const gfx::Rect& crtc_rect,
                            const gfx::Rect& src_rect,
                            const gfx::OverlayTransform transform,
                            int in_fence_fd);

  bool SetPlaneCtm(drmModeAtomicReq* property_set, uint32_t ctm_blob_id);

  void set_crtc(CrtcController* crtc) { crtc_ = crtc; }
  CrtcController* crtc() const { return crtc_; }

 private:
  CrtcController* crtc_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneAtomic);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_GPU_HARDWARE_DISPLAY_PLANE_ATOMIC_H_
