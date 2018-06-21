// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_BROWSER_XR_DEVICE_H_
#define CHROME_BROWSER_VR_SERVICE_BROWSER_XR_DEVICE_H_

#include "device/vr/vr_device.h"

namespace vr {

class VRDisplayHost;

// This class wraps the VRDevice interface, and registers for events.
// There is one BrowserXrDevice per VRDevice (ie - one per runtime).
// It manages browser-side handling of state, like which VRDisplayHost is
// listening for device activation.
class BrowserXrDevice : public device::VRDeviceEventListener {
 public:
  explicit BrowserXrDevice(device::VRDevice* device, bool is_fallback);
  ~BrowserXrDevice() override;

  device::VRDevice* GetDevice() { return device_; }

  // device::VRDeviceEventListener
  void OnChanged(device::mojom::VRDisplayInfoPtr vr_device_info) override;
  void OnExitPresent() override;
  void OnActivate(device::mojom::VRDisplayEventReason reason,
                  base::OnceCallback<void(bool)> on_handled) override;
  void OnDeactivate(device::mojom::VRDisplayEventReason reason) override;

  // Methods called by VRDisplayHost to interact with the device.
  void OnDisplayHostAdded(VRDisplayHost* display);
  void OnDisplayHostRemoved(VRDisplayHost* display);
  void ExitPresent(VRDisplayHost* display);
  void RequestSession(
      VRDisplayHost* display,
      const device::XRDeviceRuntimeSessionOptions& options,
      device::mojom::VRDisplayHost::RequestSessionCallback callback);
  VRDisplayHost* GetPresentingDisplayHost() { return presenting_display_host_; }
  void UpdateListeningForActivate(VRDisplayHost* display);
  device::mojom::VRDisplayInfoPtr GetVRDisplayInfo() {
    return display_info_.Clone();
  }

  // Methods called by VRDeviceManager to inspect the device.
  bool IsFallbackDevice() { return is_fallback_; }

 private:
  void StopExclusiveSession();
  void OnListeningForActivate(bool is_listening);
  void OnRequestSessionResult(
      VRDisplayHost* display,
      const device::XRDeviceRuntimeSessionOptions& options,
      device::mojom::VRDisplayHost::RequestSessionCallback callback,
      device::mojom::XRPresentationConnectionPtr connection,
      device::XrSessionController* exclusive_session_controller);

  // Not owned by this class, but valid while BrowserXrDevice is alive.
  device::VRDevice* device_;
  device::XrSessionController* exclusive_session_controller_ = nullptr;

  std::set<VRDisplayHost*> displays_;
  device::mojom::VRDisplayInfoPtr display_info_;

  VRDisplayHost* listening_for_activation_display_host_ = nullptr;
  VRDisplayHost* presenting_display_host_ = nullptr;
  bool is_fallback_;

  base::WeakPtrFactory<BrowserXrDevice> weak_ptr_factory_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_BROWSER_XR_DEVICE_H_
