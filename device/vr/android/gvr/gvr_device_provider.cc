// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device_provider.h"

#include "device/vr/android/gvr/gvr_device.h"

namespace device {

GvrDeviceProvider::GvrDeviceProvider() = default;
GvrDeviceProvider::~GvrDeviceProvider() = default;

void GvrDeviceProvider::Initialize(
    base::RepeatingCallback<void(unsigned int, VRDevice*)> add_device_callback,
    base::RepeatingCallback<void(unsigned int)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  vr_device_ = GvrDevice::Create();
  if (vr_device_)
    add_device_callback.Run(vr_device_->GetId(), vr_device_.get());
  initialized_ = true;
  std::move(initialization_complete).Run();
}

bool GvrDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
