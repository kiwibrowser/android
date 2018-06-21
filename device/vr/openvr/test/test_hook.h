// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_TEST_TEST_HOOK_H_
#define DEVICE_VR_OPENVR_TEST_TEST_HOOK_H_

namespace device {

// Update this string whenever either interface changes.
constexpr char kChromeOpenVRTestHookAPI[] = "ChromeTestHook_1";

struct Color {
  unsigned char r;
  unsigned char g;
  unsigned char b;
  unsigned char a;
};

struct SubmittedFrameData {
  Color color;
};

// Tests may implement this, and register it to control behavior of OpenVR.
class OpenVRTestHook {
 public:
  virtual void OnFrameSubmitted(SubmittedFrameData frame_data) = 0;
};

class TestHookRegistration {
 public:
  virtual void SetTestHook(OpenVRTestHook*) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENVR_TEST_TEST_HOOK_H_