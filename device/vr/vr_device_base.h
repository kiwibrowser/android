// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_VR_DEVICE_BASE_H
#define DEVICE_VR_VR_DEVICE_BASE_H

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_export.h"
#include "ui/display/display.h"

namespace device {

// Represents one of the platform's VR devices. Owned by the respective
// VRDeviceProvider.
// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT VRDeviceBase : public VRDevice {
 public:
  explicit VRDeviceBase(VRDeviceId id);
  ~VRDeviceBase() override;

  // VRDevice Implementation
  void PauseTracking() override;
  void ResumeTracking() override;
  void OnExitPresent() override;
  mojom::VRDisplayInfoPtr GetVRDisplayInfo() final;
  void SetMagicWindowEnabled(bool enabled) final;
  void SetVRDeviceEventListener(VRDeviceEventListener* listener) final;
  void SetListeningForActivate(bool is_listening) override;

  void GetMagicWindowPose(
      mojom::VRMagicWindowProvider::GetPoseCallback callback);
  // TODO(https://crbug.com/836478): Rename this, and probably
  // GetMagicWindowPose to GetNonExclusiveFrameData.
  void GetMagicWindowFrameData(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback);
  virtual void RequestHitTest(
      mojom::XRRayPtr ray,
      mojom::VRMagicWindowProvider::RequestHitTestCallback callback);
  unsigned int GetId() const;

  bool HasExclusiveSession();

  // TODO(https://crbug.com/845283): This method is a temporary solution
  // until a XR related refactor lands. It allows to keep using the
  // existing PauseTracking/ResumeTracking while not changing the
  // existing VR functionality.
  virtual bool ShouldPauseTrackingWhenFrameDataRestricted();

 protected:
  // Devices tell VRDeviceBase when they start presenting.  It will be paired
  // with an OnExitPresent when the device stops presenting.
  void OnStartPresenting();
  bool IsPresenting() { return presenting_; }  // Exposed for test.
  void SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info);
  void OnActivate(mojom::VRDisplayEventReason reason,
                  base::Callback<void(bool)> on_handled);

 private:
  // TODO(https://crbug.com/842227): Rename methods to HandleOnXXX
  virtual void OnListeningForActivate(bool listening);
  virtual void OnMagicWindowPoseRequest(
      mojom::VRMagicWindowProvider::GetPoseCallback callback);
  virtual void OnMagicWindowFrameDataRequest(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback);

  VRDeviceEventListener* listener_ = nullptr;

  mojom::VRDisplayInfoPtr display_info_;

  bool presenting_ = false;

  unsigned int id_;
  bool magic_window_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(VRDeviceBase);
};

}  // namespace device

#endif  // DEVICE_VR_VR_DEVICE_BASE_H
