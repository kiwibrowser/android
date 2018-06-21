// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_base.h"

#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_display_impl.h"

namespace device {

VRDeviceBase::VRDeviceBase(VRDeviceId id)
    : id_(static_cast<unsigned int>(id)) {}

VRDeviceBase::~VRDeviceBase() = default;

unsigned int VRDeviceBase::GetId() const {
  return id_;
}

void VRDeviceBase::PauseTracking() {}

void VRDeviceBase::ResumeTracking() {}

mojom::VRDisplayInfoPtr VRDeviceBase::GetVRDisplayInfo() {
  DCHECK(display_info_);
  return display_info_.Clone();
}

void VRDeviceBase::OnExitPresent() {
  if (listener_)
    listener_->OnExitPresent();
  presenting_ = false;
}

void VRDeviceBase::OnStartPresenting() {
  presenting_ = true;
}

bool VRDeviceBase::HasExclusiveSession() {
  return presenting_;
}

void VRDeviceBase::SetMagicWindowEnabled(bool enabled) {
  magic_window_enabled_ = enabled;
}

void VRDeviceBase::SetVRDeviceEventListener(VRDeviceEventListener* listener) {
  listener_ = listener;
}

void VRDeviceBase::GetMagicWindowPose(
    mojom::VRMagicWindowProvider::GetPoseCallback callback) {
  if (!magic_window_enabled_) {
    std::move(callback).Run(nullptr);
    return;
  }

  OnMagicWindowPoseRequest(std::move(callback));
}

void VRDeviceBase::GetMagicWindowFrameData(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::VRMagicWindowProvider::GetFrameDataCallback callback) {
  // TODO(https://crbug.com/836565): rename this boolean.
  if (!magic_window_enabled_) {
    std::move(callback).Run(nullptr);
    return;
  }

  OnMagicWindowFrameDataRequest(frame_size, display_rotation,
                                std::move(callback));
}

void VRDeviceBase::SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  DCHECK(display_info->index == id_);
  bool initialized = !!display_info_;
  display_info_ = std::move(display_info);

  // Don't notify when the VRDisplayInfo is initially set.
  if (!initialized)
    return;

  if (listener_)
    listener_->OnChanged(display_info_.Clone());
}

void VRDeviceBase::OnActivate(mojom::VRDisplayEventReason reason,
                              base::Callback<void(bool)> on_handled) {
  if (listener_)
    listener_->OnActivate(reason, std::move(on_handled));
}

bool VRDeviceBase::ShouldPauseTrackingWhenFrameDataRestricted() {
  return false;
}

void VRDeviceBase::OnListeningForActivate(bool listening) {}

void VRDeviceBase::OnMagicWindowPoseRequest(
    mojom::VRMagicWindowProvider::GetPoseCallback callback) {
  std::move(callback).Run(nullptr);
}

void VRDeviceBase::OnMagicWindowFrameDataRequest(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::VRMagicWindowProvider::GetFrameDataCallback callback) {
  std::move(callback).Run(nullptr);
}

void VRDeviceBase::SetListeningForActivate(bool is_listening) {
  OnListeningForActivate(is_listening);
}

void VRDeviceBase::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::VRMagicWindowProvider::RequestHitTestCallback callback) {
  NOTREACHED() << "Unexpected call to a device without hit-test support";
  std::move(callback).Run(base::nullopt);
}

}  // namespace device
