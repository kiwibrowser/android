// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_DEVICE_H
#define DEVICE_VR_OPENVR_DEVICE_H

#include <memory>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace vr {
class IVRSystem;
}  // namespace vr

namespace device {

class OpenVRRenderLoop;

class OpenVRDevice : public VRDeviceBase, public XrSessionController {
 public:
  OpenVRDevice(vr::IVRSystem* vr);
  ~OpenVRDevice() override;

  void Shutdown();

  // VRDeviceBase
  void RequestSession(const XRDeviceRuntimeSessionOptions& options,
                      VRDeviceRequestSessionCallback callback) override;

  void OnPollingEvents();

  void OnRequestSessionResult(
      VRDeviceRequestSessionCallback callback,
      bool result,
      mojom::VRSubmitFrameClientRequest request,
      mojom::VRPresentationProviderPtrInfo provider_info,
      mojom::VRDisplayFrameTransportOptionsPtr transport_options);

 private:
  // VRDeviceBase
  void OnMagicWindowPoseRequest(
      mojom::VRMagicWindowProvider::GetPoseCallback callback) override;

  // XrSessionController
  void SetFrameDataRestricted(bool restricted) override;
  void StopSession() override;

  // TODO (BillOrr): This should not be a unique_ptr because the render_loop_
  // binds to VRVSyncProvider requests, so its lifetime should be tied to the
  // lifetime of that binding.
  std::unique_ptr<OpenVRRenderLoop> render_loop_;
  mojom::VRDisplayInfoPtr display_info_;
  vr::IVRSystem* vr_system_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<OpenVRDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDevice);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_DEVICE_H
