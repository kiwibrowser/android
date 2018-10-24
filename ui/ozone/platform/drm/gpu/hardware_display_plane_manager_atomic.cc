// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/bind.h"
#include "base/files/platform_file.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/common/linux/scanout_buffer.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"

namespace ui {

namespace {

void AtomicPageFlipCallback(std::vector<base::WeakPtr<CrtcController>> crtcs,
                            unsigned int frame,
                            base::TimeTicks timestamp) {
  for (auto& crtc : crtcs) {
    auto* crtc_ptr = crtc.get();
    if (crtc_ptr)
      crtc_ptr->OnPageFlipEvent(frame, timestamp);
  }
}

}  // namespace

HardwareDisplayPlaneManagerAtomic::HardwareDisplayPlaneManagerAtomic() {
}

HardwareDisplayPlaneManagerAtomic::~HardwareDisplayPlaneManagerAtomic() {
}

bool HardwareDisplayPlaneManagerAtomic::Commit(
    HardwareDisplayPlaneList* plane_list,
    bool test_only) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (!base::ContainsValue(plane_list->plane_list, plane)) {
      // This plane is being released, so we need to zero it.
      plane->set_in_use(false);
      HardwareDisplayPlaneAtomic* atomic_plane =
          static_cast<HardwareDisplayPlaneAtomic*>(plane);
      atomic_plane->SetPlaneData(
          plane_list->atomic_property_set.get(), 0, 0, gfx::Rect(), gfx::Rect(),
          gfx::OVERLAY_TRANSFORM_NONE, base::kInvalidPlatformFile);
    }
  }

  std::vector<base::WeakPtr<CrtcController>> crtcs;
  for (HardwareDisplayPlane* plane : plane_list->plane_list) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    if (crtcs.empty() || crtcs.back().get() != atomic_plane->crtc())
      crtcs.push_back(atomic_plane->crtc()->AsWeakPtr());
  }

  if (test_only) {
    for (HardwareDisplayPlane* plane : plane_list->plane_list) {
      plane->set_in_use(false);
    }
  } else {
    plane_list->plane_list.swap(plane_list->old_plane_list);
  }

  uint32_t flags = 0;
  if (test_only) {
    flags = DRM_MODE_ATOMIC_TEST_ONLY;
  } else {
    flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;
  }

  if (!drm_->CommitProperties(plane_list->atomic_property_set.get(), flags,
                              crtcs.size(),
                              base::BindOnce(&AtomicPageFlipCallback, crtcs))) {
    if (!test_only) {
      PLOG(ERROR) << "Failed to commit properties for page flip.";
    } else {
      VPLOG(2) << "Failed to commit properties for MODE_ATOMIC_TEST_ONLY.";
    }

    ResetCurrentPlaneList(plane_list);
    return false;
  }

  plane_list->plane_list.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return true;
}

bool HardwareDisplayPlaneManagerAtomic::DisableOverlayPlanes(
    HardwareDisplayPlaneList* plane_list) {
  for (HardwareDisplayPlane* plane : plane_list->old_plane_list) {
    if (plane->type() != HardwareDisplayPlane::kOverlay)
      continue;
    plane->set_in_use(false);
    plane->set_owning_crtc(0);

    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane);
    atomic_plane->SetPlaneData(
        plane_list->atomic_property_set.get(), 0, 0, gfx::Rect(), gfx::Rect(),
        gfx::OVERLAY_TRANSFORM_NONE, base::kInvalidPlatformFile);
  }
  // The list of crtcs is only useful if flags contains DRM_MODE_PAGE_FLIP_EVENT
  // to get the pageflip callback. In this case we don't need to be notified
  // at the next page flip, so the list of crtcs can be empty.
  std::vector<base::WeakPtr<CrtcController>> crtcs;
  bool ret = drm_->CommitProperties(
      plane_list->atomic_property_set.get(), DRM_MODE_ATOMIC_NONBLOCK,
      crtcs.size(), base::BindOnce(&AtomicPageFlipCallback, crtcs));
  PLOG_IF(ERROR, !ret) << "Failed to commit properties for page flip.";

  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
  return ret;
}

bool HardwareDisplayPlaneManagerAtomic::SetColorCorrectionOnAllCrtcPlanes(
    uint32_t crtc_id,
    ScopedDrmColorCtmPtr ctm_blob_data) {
  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());
  ScopedDrmPropertyBlob property_blob(
      drm_->CreatePropertyBlob(ctm_blob_data.get(), sizeof(drm_color_ctm)));

  const int crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK_GE(crtc_index, 0);

  for (auto& plane : planes_) {
    HardwareDisplayPlaneAtomic* atomic_plane =
        static_cast<HardwareDisplayPlaneAtomic*>(plane.get());

    // This assumes planes can only belong to one crtc.
    if (!atomic_plane->CanUseForCrtc(crtc_index))
      continue;

    if (!atomic_plane->SetPlaneCtm(property_set.get(), property_blob->id())) {
      LOG(ERROR) << "Failed to set PLANE_CTM for plane=" << atomic_plane->id();
      return false;
    }
  }

  return drm_->CommitProperties(property_set.get(), DRM_MODE_ATOMIC_NONBLOCK, 0,
                                DrmDevice::PageFlipCallback());
}

bool HardwareDisplayPlaneManagerAtomic::ValidatePrimarySize(
    const OverlayPlane& primary,
    const drmModeModeInfo& mode) {
  // Atomic KMS allows for primary planes that don't match the size of
  // the current mode.
  return true;
}

void HardwareDisplayPlaneManagerAtomic::RequestPlanesReadyCallback(
    const OverlayPlaneList& planes,
    base::OnceClosure callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));
}

bool HardwareDisplayPlaneManagerAtomic::SetPlaneData(
    HardwareDisplayPlaneList* plane_list,
    HardwareDisplayPlane* hw_plane,
    const OverlayPlane& overlay,
    uint32_t crtc_id,
    const gfx::Rect& src_rect,
    CrtcController* crtc) {
  HardwareDisplayPlaneAtomic* atomic_plane =
      static_cast<HardwareDisplayPlaneAtomic*>(hw_plane);
  uint32_t framebuffer_id = overlay.enable_blend
                                ? overlay.buffer->GetFramebufferId()
                                : overlay.buffer->GetOpaqueFramebufferId();
  int fence_fd = base::kInvalidPlatformFile;

  if (overlay.gpu_fence) {
    const auto& gpu_fence_handle = overlay.gpu_fence->GetGpuFenceHandle();
    if (gpu_fence_handle.type !=
        gfx::GpuFenceHandleType::kAndroidNativeFenceSync) {
      LOG(ERROR) << "Received invalid gpu fence";
      return false;
    }
    fence_fd = gpu_fence_handle.native_fd.fd;
  }

  if (!atomic_plane->SetPlaneData(plane_list->atomic_property_set.get(),
                                  crtc_id, framebuffer_id,
                                  overlay.display_bounds, src_rect,
                                  overlay.plane_transform, fence_fd)) {
    LOG(ERROR) << "Failed to set plane properties";
    return false;
  }
  atomic_plane->set_crtc(crtc);
  return true;
}

std::unique_ptr<HardwareDisplayPlane>
HardwareDisplayPlaneManagerAtomic::CreatePlane(uint32_t plane_id) {
  return std::make_unique<HardwareDisplayPlaneAtomic>(plane_id);
}

bool HardwareDisplayPlaneManagerAtomic::CommitColorMatrix(
    const CrtcProperties& crtc_props) {
  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());
  int ret = drmModeAtomicAddProperty(property_set.get(), crtc_props.id,
                                     crtc_props.ctm.id, crtc_props.ctm.value);
  if (ret < 0) {
    LOG(ERROR) << "Failed to set CTM property for crtc=" << crtc_props.id;
    return false;
  }

  // If we try to do this in a non-blocking fashion this can return EBUSY since
  // there is a pending page flip. Do a blocking commit (the same as the legacy
  // API) to ensure the properties are applied.
  // TODO(dnicoara): Should cache these values locally and aggregate them with
  // the page flip event otherwise this "steals" a vsync to apply the property.
  return drm_->CommitProperties(property_set.get(), 0, 0,
                                DrmDevice::PageFlipCallback());
}

bool HardwareDisplayPlaneManagerAtomic::CommitGammaCorrection(
    const CrtcProperties& crtc_props) {
  DCHECK(crtc_props.degamma_lut.id || crtc_props.gamma_lut.id);

  ScopedDrmAtomicReqPtr property_set(drmModeAtomicAlloc());
  if (crtc_props.degamma_lut.id) {
    int ret = drmModeAtomicAddProperty(property_set.get(), crtc_props.id,
                                       crtc_props.degamma_lut.id,
                                       crtc_props.degamma_lut.value);
    if (ret < 0) {
      LOG(ERROR) << "Failed to set DEGAMMA_LUT property for crtc="
                 << crtc_props.id;
      return false;
    }
  }

  if (crtc_props.gamma_lut.id) {
    int ret = drmModeAtomicAddProperty(property_set.get(), crtc_props.id,
                                       crtc_props.gamma_lut.id,
                                       crtc_props.gamma_lut.value);
    if (ret < 0) {
      LOG(ERROR) << "Failed to set GAMMA_LUT property for crtc="
                 << crtc_props.id;
      return false;
    }
  }

  // If we try to do this in a non-blocking fashion this can return EBUSY since
  // there is a pending page flip. Do a blocking commit (the same as the legacy
  // API) to ensure the properties are applied.
  // TODO(dnicoara): Should cache these values locally and aggregate them with
  // the page flip event otherwise this "steals" a vsync to apply the property.
  return drm_->CommitProperties(property_set.get(), 0, 0,
                                DrmDevice::PageFlipCallback());
}

}  // namespace ui
