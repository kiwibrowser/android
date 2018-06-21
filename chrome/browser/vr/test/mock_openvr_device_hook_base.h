// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_OPENVR_DEVICE_HOOK_BASE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_OPENVR_DEVICE_HOOK_BASE_H_

#include "device/vr/openvr/openvr_device_provider.h"
#include "device/vr/openvr/test/test_hook.h"

class MockOpenVRBase : public device::OpenVRTestHook {
 public:
  MockOpenVRBase();
  virtual ~MockOpenVRBase();

  // OpenVRTestHook
  void OnFrameSubmitted(device::SubmittedFrameData frame_data) override;
};

#endif  // CHROME_BROWSER_VR_TEST_MOCK_OPENVR_DEVICE_HOOK_BASE_H_
