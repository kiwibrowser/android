// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

#include <drm_fourcc.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/common/linux/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_dummy.h"

namespace ui {
namespace {

constexpr float kFixedPointScaleValue = 1 << 16;

}  // namespace

HardwareDisplayPlaneList::HardwareDisplayPlaneList() {
  atomic_property_set.reset(drmModeAtomicAlloc());
}

HardwareDisplayPlaneList::~HardwareDisplayPlaneList() {
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(uint32_t crtc_id,
                                                     uint32_t framebuffer,
                                                     CrtcController* crtc)
    : crtc_id(crtc_id), framebuffer(framebuffer), crtc(crtc) {
}

HardwareDisplayPlaneList::PageFlipInfo::PageFlipInfo(
    const PageFlipInfo& other) = default;

HardwareDisplayPlaneList::PageFlipInfo::~PageFlipInfo() {
}

HardwareDisplayPlaneList::PageFlipInfo::Plane::Plane(int plane,
                                                     int framebuffer,
                                                     const gfx::Rect& bounds,
                                                     const gfx::Rect& src_rect)
    : plane(plane),
      framebuffer(framebuffer),
      bounds(bounds),
      src_rect(src_rect) {
}

HardwareDisplayPlaneList::PageFlipInfo::Plane::~Plane() {
}

HardwareDisplayPlaneManager::HardwareDisplayPlaneManager() : drm_(nullptr) {
}

HardwareDisplayPlaneManager::~HardwareDisplayPlaneManager() {
}

bool HardwareDisplayPlaneManager::Initialize(DrmDevice* drm) {
  drm_ = drm;

  bool has_universal_planes = false;
// Try to get all of the planes if possible, so we don't have to try to
// discover hidden primary planes.
#if defined(DRM_CLIENT_CAP_UNIVERSAL_PLANES)
  has_universal_planes =
      drm_->SetCapability(DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
#endif

  if (!InitializeCrtcProperties(drm))
    return false;

  ScopedDrmPlaneResPtr plane_resources = drm->GetPlaneResources();
  if (!plane_resources) {
    PLOG(ERROR) << "Failed to get plane resources.";
    return false;
  }

  std::set<uint32_t> plane_ids;
  for (uint32_t i = 0; i < plane_resources->count_planes; ++i) {
    plane_ids.insert(plane_resources->planes[i]);
    std::unique_ptr<HardwareDisplayPlane> plane(
        CreatePlane(plane_resources->planes[i]));

    if (plane->Initialize(drm))
      planes_.push_back(std::move(plane));
  }

  // crbug.com/464085: if driver reports no primary planes for a crtc, create a
  // dummy plane for which we can assign exactly one overlay.
  // TODO(dnicoara): refactor this to simplify AssignOverlayPlanes and move
  // this workaround into HardwareDisplayPlaneLegacy.
  if (!has_universal_planes) {
    for (size_t i = 0; i < crtc_properties_.size(); ++i) {
      if (plane_ids.find(crtc_properties_[i].id - 1) == plane_ids.end()) {
        std::unique_ptr<HardwareDisplayPlane> dummy_plane(
            new HardwareDisplayPlaneDummy(crtc_properties_[i].id - 1, 1 << i));
        if (dummy_plane->Initialize(drm)) {
          planes_.push_back(std::move(dummy_plane));
        }
      }
    }
  }

  std::sort(planes_.begin(), planes_.end(),
            [](const std::unique_ptr<HardwareDisplayPlane>& l,
               const std::unique_ptr<HardwareDisplayPlane>& r) {
              return l->id() < r->id();
            });

  PopulateSupportedFormats();
  return true;
}

std::unique_ptr<HardwareDisplayPlane> HardwareDisplayPlaneManager::CreatePlane(
    uint32_t id) {
  return std::make_unique<HardwareDisplayPlane>(id);
}

HardwareDisplayPlane* HardwareDisplayPlaneManager::FindNextUnusedPlane(
    size_t* index,
    uint32_t crtc_index,
    const OverlayPlane& overlay) const {
  for (size_t i = *index; i < planes_.size(); ++i) {
    auto* plane = planes_[i].get();
    if (!plane->in_use() && IsCompatible(plane, overlay, crtc_index)) {
      *index = i + 1;
      return plane;
    }
  }
  return nullptr;
}

int HardwareDisplayPlaneManager::LookupCrtcIndex(uint32_t crtc_id) const {
  for (size_t i = 0; i < crtc_properties_.size(); ++i)
    if (crtc_properties_[i].id == crtc_id)
      return i;
  return -1;
}

bool HardwareDisplayPlaneManager::IsCompatible(HardwareDisplayPlane* plane,
                                               const OverlayPlane& overlay,
                                               uint32_t crtc_index) const {
  if (plane->type() == HardwareDisplayPlane::kCursor ||
      !plane->CanUseForCrtc(crtc_index))
    return false;

  const uint32_t format = overlay.enable_blend ?
      overlay.buffer->GetFramebufferPixelFormat() :
      overlay.buffer->GetOpaqueFramebufferPixelFormat();
  if (!plane->IsSupportedFormat(format))
    return false;

  // TODO(kalyank): We should check for z-order and any needed transformation
  // support. Driver doesn't expose any property to check for z-order, can we
  // rely on the sorting we do based on plane ids ?

  return true;
}

void HardwareDisplayPlaneManager::PopulateSupportedFormats() {
  std::set<uint32_t> supported_formats;

  for (const auto& plane : planes_) {
    const std::vector<uint32_t>& formats = plane->supported_formats();
    supported_formats.insert(formats.begin(), formats.end());
  }

  supported_formats_.reserve(supported_formats.size());
  supported_formats_.assign(supported_formats.begin(), supported_formats.end());
}

void HardwareDisplayPlaneManager::ResetCurrentPlaneList(
    HardwareDisplayPlaneList* plane_list) const {
  for (auto* hardware_plane : plane_list->plane_list) {
    hardware_plane->set_in_use(false);
    hardware_plane->set_owning_crtc(0);
  }

  plane_list->plane_list.clear();
  plane_list->legacy_page_flips.clear();
  plane_list->atomic_property_set.reset(drmModeAtomicAlloc());
}

void HardwareDisplayPlaneManager::BeginFrame(
    HardwareDisplayPlaneList* plane_list) {
  for (auto* plane : plane_list->old_plane_list) {
    plane->set_in_use(false);
  }
}

bool HardwareDisplayPlaneManager::AssignOverlayPlanes(
    HardwareDisplayPlaneList* plane_list,
    const OverlayPlaneList& overlay_list,
    uint32_t crtc_id,
    CrtcController* crtc) {
  int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Cannot find crtc " << crtc_id;
    return false;
  }

  size_t plane_idx = 0;
  for (const auto& plane : overlay_list) {
    HardwareDisplayPlane* hw_plane =
        FindNextUnusedPlane(&plane_idx, crtc_index, plane);
    if (!hw_plane) {
      LOG(ERROR) << "Failed to find a free plane for crtc " << crtc_id;
      ResetCurrentPlaneList(plane_list);
      return false;
    }

    gfx::Rect fixed_point_rect;
    if (hw_plane->type() != HardwareDisplayPlane::kDummy) {
      const gfx::Size& size = plane.buffer->GetSize();
      gfx::RectF crop_rect = plane.crop_rect;
      crop_rect.Scale(size.width(), size.height());

      // This returns a number in 16.16 fixed point, required by the DRM overlay
      // APIs.
      auto to_fixed_point =
          [](double v) -> uint32_t { return v * kFixedPointScaleValue; };
      fixed_point_rect = gfx::Rect(to_fixed_point(crop_rect.x()),
                                   to_fixed_point(crop_rect.y()),
                                   to_fixed_point(crop_rect.width()),
                                   to_fixed_point(crop_rect.height()));
    }

    if (!SetPlaneData(plane_list, hw_plane, plane, crtc_id, fixed_point_rect,
                      crtc)) {
      ResetCurrentPlaneList(plane_list);
      return false;
    }

    plane_list->plane_list.push_back(hw_plane);
    hw_plane->set_owning_crtc(crtc_id);
    hw_plane->set_in_use(true);
  }
  return true;
}

const std::vector<uint32_t>& HardwareDisplayPlaneManager::GetSupportedFormats()
    const {
  return supported_formats_;
}

std::vector<uint64_t> HardwareDisplayPlaneManager::GetFormatModifiers(
    uint32_t crtc_id,
    uint32_t format) {
  int crtc_index = LookupCrtcIndex(crtc_id);

  for (const auto& plane : planes_) {
    if (plane->CanUseForCrtc(crtc_index) &&
        plane->type() == HardwareDisplayPlane::kPrimary) {
      return plane->ModifiersForFormat(format);
    }
  }

  return std::vector<uint64_t>();
}

bool HardwareDisplayPlaneManager::SetColorMatrix(
    uint32_t crtc_id,
    const std::vector<float>& color_matrix) {
  if (color_matrix.empty()) {
    // TODO: Consider allowing an empty matrix to disable the color transform
    // matrix.
    LOG(ERROR) << "CTM is empty. Expected a 3x3 matrix.";
    return false;
  }

  const int crtc_index = LookupCrtcIndex(crtc_id);
  DCHECK_GE(crtc_index, 0);
  CrtcProperties* crtc_props = &crtc_properties_[crtc_index];

  ScopedDrmColorCtmPtr ctm_blob_data = CreateCTMBlob(color_matrix);
  if (!crtc_props->ctm.id)
    return SetColorCorrectionOnAllCrtcPlanes(crtc_id, std::move(ctm_blob_data));

  ScopedDrmPropertyBlob ctm_prop =
      drm_->CreatePropertyBlob(ctm_blob_data.get(), sizeof(drm_color_ctm));
  crtc_props->ctm.value = ctm_prop->id();
  return CommitColorMatrix(*crtc_props);
}

bool HardwareDisplayPlaneManager::SetGammaCorrection(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  const int crtc_index = LookupCrtcIndex(crtc_id);
  if (crtc_index < 0) {
    LOG(ERROR) << "Unknown CRTC ID=" << crtc_id;
    return false;
  }

  CrtcProperties* crtc_props = &crtc_properties_[crtc_index];

  if (!degamma_lut.empty() &&
      (!crtc_props->degamma_lut.id || !crtc_props->degamma_lut_size.id))
    return false;

  if (!gamma_lut.empty() &&
      (!crtc_props->gamma_lut.id || !crtc_props->gamma_lut_size.id)) {
    // If we can't find the degamma & gamma lut, it means the properties
    // aren't available. We should then try to use the legacy gamma ramp ioctl.
    if (degamma_lut.empty())
      return drm_->SetGammaRamp(crtc_id, gamma_lut);

    // We're missing either degamma or gamma lut properties. We shouldn't try to
    // set just one of them.
    return false;
  }

  ScopedDrmColorLutPtr degamma_blob_data = CreateLutBlob(
      ResampleLut(degamma_lut, crtc_props->degamma_lut_size.value));
  ScopedDrmColorLutPtr gamma_blob_data =
      CreateLutBlob(ResampleLut(gamma_lut, crtc_props->gamma_lut_size.value));

  ScopedDrmPropertyBlob degamma_prop, gamma_prop;
  if (degamma_blob_data) {
    degamma_prop = drm_->CreatePropertyBlob(
        degamma_blob_data.get(),
        sizeof(drm_color_lut) * crtc_props->degamma_lut_size.value);
    crtc_props->degamma_lut.value = degamma_prop->id();
  } else {
    crtc_props->degamma_lut.value = 0;
  }

  if (gamma_blob_data) {
    gamma_prop = drm_->CreatePropertyBlob(
        gamma_blob_data.get(),
        sizeof(drm_color_lut) * crtc_props->gamma_lut_size.value);
    crtc_props->gamma_lut.value = gamma_prop->id();
  } else {
    crtc_props->gamma_lut.value = 0;
  }

  return CommitGammaCorrection(*crtc_props);
}

bool HardwareDisplayPlaneManager::InitializeCrtcProperties(DrmDevice* drm) {
  ScopedDrmResourcesPtr resources(drm->GetResources());
  if (!resources) {
    PLOG(ERROR) << "Failed to get resources.";
    return false;
  }

  for (int i = 0; i < resources->count_crtcs; ++i) {
    CrtcProperties p{};
    p.id = resources->crtcs[i];

    ScopedDrmObjectPropertyPtr props(
        drm->GetObjectProperties(resources->crtcs[i], DRM_MODE_OBJECT_CRTC));
    if (!props) {
      PLOG(ERROR) << "Failed to get CRTC properties for crtc_id=" << p.id;
      continue;
    }

    // These properties are optional. If they don't exist we can tell by the
    // invalid ID.
    GetDrmPropertyForName(drm, props.get(), "CTM", &p.ctm);
    GetDrmPropertyForName(drm, props.get(), "GAMMA_LUT", &p.gamma_lut);
    GetDrmPropertyForName(drm, props.get(), "GAMMA_LUT_SIZE",
                          &p.gamma_lut_size);
    GetDrmPropertyForName(drm, props.get(), "DEGAMMA_LUT", &p.degamma_lut);
    GetDrmPropertyForName(drm, props.get(), "DEGAMMA_LUT_SIZE",
                          &p.degamma_lut_size);

    crtc_properties_.push_back(p);
  }

  return true;
}

}  // namespace ui
