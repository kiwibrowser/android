// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"

#include <xf86drm.h>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

template <class Object>
Object* DrmAllocator() {
  return static_cast<Object*>(drmMalloc(sizeof(Object)));
}

ScopedDrmObjectPropertyPtr CreatePropertyObject(
    const std::vector<DrmDevice::Property>& properties) {
  ScopedDrmObjectPropertyPtr drm_properties(
      DrmAllocator<drmModeObjectProperties>());
  drm_properties->count_props = properties.size();
  drm_properties->props = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * drm_properties->count_props));
  drm_properties->prop_values = static_cast<uint64_t*>(
      drmMalloc(sizeof(uint64_t) * drm_properties->count_props));
  for (size_t i = 0; i < properties.size(); ++i) {
    drm_properties->props[i] = properties[i].id;
    drm_properties->prop_values[i] = properties[i].value;
  }

  return drm_properties;
}

}  // namespace

MockDrmDevice::CrtcProperties::CrtcProperties() = default;
MockDrmDevice::CrtcProperties::CrtcProperties(const CrtcProperties&) = default;
MockDrmDevice::CrtcProperties::~CrtcProperties() = default;

MockDrmDevice::PlaneProperties::PlaneProperties() = default;
MockDrmDevice::PlaneProperties::PlaneProperties(const PlaneProperties&) =
    default;
MockDrmDevice::PlaneProperties::~PlaneProperties() = default;

MockDrmDevice::MockDrmDevice(bool use_sync_flips)
    : DrmDevice(base::FilePath(), base::File(), true /* is_primary_device */),
      get_crtc_call_count_(0),
      set_crtc_call_count_(0),
      restore_crtc_call_count_(0),
      add_framebuffer_call_count_(0),
      remove_framebuffer_call_count_(0),
      page_flip_call_count_(0),
      overlay_flip_call_count_(0),
      overlay_clear_call_count_(0),
      allocate_buffer_count_(0),
      set_crtc_expectation_(true),
      add_framebuffer_expectation_(true),
      page_flip_expectation_(true),
      create_dumb_buffer_expectation_(true),
      use_sync_flips_(use_sync_flips),
      current_framebuffer_(0) {
  plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
}

// static
ScopedDrmPropertyBlobPtr MockDrmDevice::AllocateInFormatsBlob(
    uint32_t id,
    const std::vector<uint32_t>& supported_formats,
    const std::vector<drm_format_modifier>& supported_format_modifiers) {
  drm_format_modifier_blob header;
  header.count_formats = supported_formats.size();
  header.formats_offset = sizeof(header);
  header.count_modifiers = supported_format_modifiers.size();
  header.modifiers_offset =
      header.formats_offset + sizeof(uint32_t) * header.count_formats;

  ScopedDrmPropertyBlobPtr blob(DrmAllocator<drmModePropertyBlobRes>());
  blob->id = id;
  blob->length = header.modifiers_offset +
                 sizeof(drm_format_modifier) * header.count_modifiers;
  blob->data = drmMalloc(blob->length);

  memcpy(blob->data, &header, sizeof(header));
  memcpy(static_cast<uint8_t*>(blob->data) + header.formats_offset,
         supported_formats.data(), sizeof(uint32_t) * header.count_formats);
  memcpy(static_cast<uint8_t*>(blob->data) + header.modifiers_offset,
         supported_format_modifiers.data(),
         sizeof(drm_format_modifier) * header.count_modifiers);

  return blob;
}

void MockDrmDevice::InitializeState(
    const std::vector<CrtcProperties>& crtc_properties,
    const std::vector<PlaneProperties>& plane_properties,
    const std::map<uint32_t, std::string>& property_names,
    bool use_atomic) {
  crtc_properties_ = crtc_properties;
  plane_properties_ = plane_properties;
  property_names_ = property_names;
  if (use_atomic) {
    plane_manager_.reset(new HardwareDisplayPlaneManagerAtomic());
  } else {
    plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
  }

  CHECK(plane_manager_->Initialize(this));
}

MockDrmDevice::~MockDrmDevice() {}

ScopedDrmResourcesPtr MockDrmDevice::GetResources() {
  ScopedDrmResourcesPtr resources(DrmAllocator<drmModeRes>());
  resources->count_crtcs = crtc_properties_.size();
  resources->crtcs = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_crtcs));
  for (size_t i = 0; i < crtc_properties_.size(); ++i)
    resources->crtcs[i] = crtc_properties_[i].id;

  return resources;
}

ScopedDrmPlaneResPtr MockDrmDevice::GetPlaneResources() {
  ScopedDrmPlaneResPtr resources(DrmAllocator<drmModePlaneRes>());
  resources->count_planes = plane_properties_.size();
  resources->planes = static_cast<uint32_t*>(
      drmMalloc(sizeof(uint32_t) * resources->count_planes));
  for (size_t i = 0; i < plane_properties_.size(); ++i)
    resources->planes[i] = plane_properties_[i].id;

  return resources;
}

ScopedDrmObjectPropertyPtr MockDrmDevice::GetObjectProperties(
    uint32_t object_id,
    uint32_t object_type) {
  if (object_type == DRM_MODE_OBJECT_PLANE) {
    auto it = std::find_if(
        plane_properties_.begin(), plane_properties_.end(),
        [object_id](const PlaneProperties& p) { return p.id == object_id; });
    if (it == plane_properties_.end())
      return nullptr;

    return CreatePropertyObject(it->properties);
  } else if (object_type == DRM_MODE_OBJECT_CRTC) {
    auto it = std::find_if(
        crtc_properties_.begin(), crtc_properties_.end(),
        [object_id](const CrtcProperties& p) { return p.id == object_id; });
    if (it == crtc_properties_.end())
      return nullptr;

    return CreatePropertyObject(it->properties);
  }

  return nullptr;
}

ScopedDrmCrtcPtr MockDrmDevice::GetCrtc(uint32_t crtc_id) {
  get_crtc_call_count_++;
  return ScopedDrmCrtcPtr(DrmAllocator<drmModeCrtc>());
}

bool MockDrmDevice::SetCrtc(uint32_t crtc_id,
                            uint32_t framebuffer,
                            std::vector<uint32_t> connectors,
                            drmModeModeInfo* mode) {
  current_framebuffer_ = framebuffer;
  set_crtc_call_count_++;
  return set_crtc_expectation_;
}

bool MockDrmDevice::SetCrtc(drmModeCrtc* crtc,
                            std::vector<uint32_t> connectors) {
  restore_crtc_call_count_++;
  return true;
}

bool MockDrmDevice::DisableCrtc(uint32_t crtc_id) {
  current_framebuffer_ = 0;
  return true;
}

ScopedDrmConnectorPtr MockDrmDevice::GetConnector(uint32_t connector_id) {
  return ScopedDrmConnectorPtr(DrmAllocator<drmModeConnector>());
}

bool MockDrmDevice::AddFramebuffer2(uint32_t width,
                                    uint32_t height,
                                    uint32_t format,
                                    uint32_t handles[4],
                                    uint32_t strides[4],
                                    uint32_t offsets[4],
                                    uint64_t modifiers[4],
                                    uint32_t* framebuffer,
                                    uint32_t flags) {
  add_framebuffer_call_count_++;
  *framebuffer = add_framebuffer_call_count_;
  return add_framebuffer_expectation_;
}

bool MockDrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  remove_framebuffer_call_count_++;
  return true;
}

ScopedDrmFramebufferPtr MockDrmDevice::GetFramebuffer(uint32_t framebuffer) {
  return ScopedDrmFramebufferPtr();
}

bool MockDrmDevice::PageFlip(uint32_t crtc_id,
                             uint32_t framebuffer,
                             PageFlipCallback callback) {
  page_flip_call_count_++;
  current_framebuffer_ = framebuffer;
  if (page_flip_expectation_) {
    if (use_sync_flips_)
      std::move(callback).Run(0, base::TimeTicks());
    else
      callbacks_.push(std::move(callback));
  }

  return page_flip_expectation_;
}

bool MockDrmDevice::PageFlipOverlay(uint32_t crtc_id,
                                    uint32_t framebuffer,
                                    const gfx::Rect& location,
                                    const gfx::Rect& source,
                                    int overlay_plane) {
  if (!framebuffer)
    overlay_clear_call_count_++;
  overlay_flip_call_count_++;
  return true;
}

ScopedDrmPlanePtr MockDrmDevice::GetPlane(uint32_t plane_id) {
  auto it = std::find_if(plane_properties_.begin(), plane_properties_.end(),
                         [plane_id](const PlaneProperties& plane) {
                           return plane.id == plane_id;
                         });
  if (it == plane_properties_.end())
    return nullptr;

  ScopedDrmPlanePtr plane(DrmAllocator<drmModePlane>());
  plane->possible_crtcs = it->crtc_mask;
  return plane;
}

ScopedDrmPropertyPtr MockDrmDevice::GetProperty(drmModeConnector* connector,
                                                const char* name) {
  return ScopedDrmPropertyPtr(DrmAllocator<drmModePropertyRes>());
}

ScopedDrmPropertyPtr MockDrmDevice::GetProperty(uint32_t id) {
  auto it = property_names_.find(id);
  if (it == property_names_.end())
    return nullptr;

  ScopedDrmPropertyPtr property(DrmAllocator<drmModePropertyRes>());
  property->prop_id = id;
  strcpy(property->name, it->second.c_str());
  return property;
}

bool MockDrmDevice::SetProperty(uint32_t connector_id,
                                uint32_t property_id,
                                uint64_t value) {
  return true;
}

ScopedDrmPropertyBlob MockDrmDevice::CreatePropertyBlob(void* blob,
                                                        size_t size) {
  return ScopedDrmPropertyBlob(new DrmPropertyBlobMetadata(this, 0xffffffff));
}

void MockDrmDevice::DestroyPropertyBlob(uint32_t id) {}

bool MockDrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  return true;
}

ScopedDrmPropertyBlobPtr MockDrmDevice::GetPropertyBlob(uint32_t property_id) {
  auto it = blob_property_map_.find(property_id);
  if (it == blob_property_map_.end())
    return nullptr;

  ScopedDrmPropertyBlobPtr blob(DrmAllocator<drmModePropertyBlobRes>());
  blob->id = property_id;
  blob->length = it->second->length;
  blob->data = drmMalloc(blob->length);
  memcpy(blob->data, it->second->data, blob->length);

  return blob;
}

ScopedDrmPropertyBlobPtr MockDrmDevice::GetPropertyBlob(
    drmModeConnector* connector,
    const char* name) {
  return ScopedDrmPropertyBlobPtr(DrmAllocator<drmModePropertyBlobRes>());
}

bool MockDrmDevice::SetObjectProperty(uint32_t object_id,
                                      uint32_t object_type,
                                      uint32_t property_id,
                                      uint32_t property_value) {
  set_object_property_count_++;
  return true;
}

bool MockDrmDevice::SetCursor(uint32_t crtc_id,
                              uint32_t handle,
                              const gfx::Size& size) {
  crtc_cursor_map_[crtc_id] = handle;
  return true;
}

bool MockDrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  return true;
}

bool MockDrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                     uint32_t* handle,
                                     uint32_t* stride) {
  if (!create_dumb_buffer_expectation_)
    return false;

  *handle = allocate_buffer_count_++;
  *stride = info.minRowBytes();
  void* pixels = new char[info.computeByteSize(*stride)];
  buffers_.push_back(SkSurface::MakeRasterDirect(info, pixels, *stride));
  buffers_[*handle]->getCanvas()->clear(SK_ColorBLACK);

  return true;
}

bool MockDrmDevice::DestroyDumbBuffer(uint32_t handle) {
  if (handle >= buffers_.size() || !buffers_[handle])
    return false;

  buffers_[handle].reset();
  return true;
}

bool MockDrmDevice::MapDumbBuffer(uint32_t handle, size_t size, void** pixels) {
  if (handle >= buffers_.size() || !buffers_[handle])
    return false;

  SkPixmap pixmap;
  buffers_[handle]->peekPixels(&pixmap);
  *pixels = const_cast<void*>(pixmap.addr());
  return true;
}

bool MockDrmDevice::UnmapDumbBuffer(void* pixels, size_t size) {
  return true;
}

bool MockDrmDevice::CloseBufferHandle(uint32_t handle) {
  return true;
}

bool MockDrmDevice::CommitProperties(drmModeAtomicReq* properties,
                                     uint32_t flags,
                                     uint32_t crtc_count,
                                     PageFlipCallback callback) {
  if (use_sync_flips_)
    std::move(callback).Run(0, base::TimeTicks());
  else
    callbacks_.push(std::move(callback));

  commit_count_++;
  return true;
}

bool MockDrmDevice::SetGammaRamp(
    uint32_t crtc_id,
    const std::vector<display::GammaRampRGBEntry>& lut) {
  return legacy_gamma_ramp_expectation_;
}

bool MockDrmDevice::SetCapability(uint64_t capability, uint64_t value) {
  return true;
}

void MockDrmDevice::RunCallbacks() {
  while (!callbacks_.empty()) {
    PageFlipCallback callback = std::move(callbacks_.front());
    callbacks_.pop();
    std::move(callback).Run(0, base::TimeTicks());
  }
}

void MockDrmDevice::SetPropertyBlob(ScopedDrmPropertyBlobPtr blob) {
  blob_property_map_[blob->id] = std::move(blob);
}

}  // namespace ui
