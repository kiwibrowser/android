// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"

MockOpenVRBase::MockOpenVRBase() {
  device::OpenVRDeviceProvider::SetTestHook(this);
}

MockOpenVRBase::~MockOpenVRBase() {
  device::OpenVRDeviceProvider::SetTestHook(nullptr);
}

void MockOpenVRBase::OnFrameSubmitted(device::SubmittedFrameData frame_data) {}
