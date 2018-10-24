// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_DEVICE_PROVIDER_H
#define DEVICE_VR_OPENVR_DEVICE_PROVIDER_H

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"

namespace device {

class OpenVRDevice;
class OpenVRTestHook;
class TestHookRegistration;

class DEVICE_VR_EXPORT OpenVRDeviceProvider : public VRDeviceProvider {
 public:
  OpenVRDeviceProvider();
  ~OpenVRDeviceProvider() override;

  void Initialize(
      base::RepeatingCallback<void(unsigned int, VRDevice*)>
          add_device_callback,
      base::RepeatingCallback<void(unsigned int)> remove_device_callback,
      base::OnceClosure initialization_complete) override;

  bool Initialized() override;

  static void RecordRuntimeAvailability();

  static void SetTestHook(OpenVRTestHook*);

 private:
  void CreateDevice();

  std::unique_ptr<OpenVRDevice> device_;
  bool initialized_ = false;
  static OpenVRTestHook* test_hook_s;
  static TestHookRegistration* test_hook_registration_s;

  DISALLOW_COPY_AND_ASSIGN(OpenVRDeviceProvider);
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_DEVICE_PROVIDER_H
