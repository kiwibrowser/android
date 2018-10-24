// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_ANDROID_GVR_DEVICE_H
#define DEVICE_VR_ANDROID_GVR_DEVICE_H

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "device/vr/vr_device_base.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"

namespace device {

class GvrDelegateProvider;

// TODO(mthiesse, crbug.com/769373): Remove DEVICE_VR_EXPORT.
class DEVICE_VR_EXPORT GvrDevice : public VRDeviceBase,
                                   public XrSessionController {
 public:
  static std::unique_ptr<GvrDevice> Create();
  ~GvrDevice() override;

  // VRDeviceBase
  void RequestSession(const XRDeviceRuntimeSessionOptions& options,
                      VRDeviceRequestSessionCallback callback) override;
  void PauseTracking() override;
  void ResumeTracking() override;

  void OnDisplayConfigurationChanged(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj);

  void Activate(mojom::VRDisplayEventReason reason,
                base::Callback<void(bool)> on_handled);

 private:
  // VRDeviceBase
  void OnListeningForActivate(bool listening) override;
  void OnMagicWindowPoseRequest(
      mojom::VRMagicWindowProvider::GetPoseCallback callback) override;

  void OnRequestSessionResult(VRDeviceRequestSessionCallback callback,
                              mojom::XRPresentationConnectionPtr connection);

  // XrSessionController
  void SetFrameDataRestricted(bool restricted) override;
  void StopSession() override;

  GvrDevice();
  GvrDelegateProvider* GetGvrDelegateProvider();

  base::android::ScopedJavaGlobalRef<jobject> non_presenting_context_;
  std::unique_ptr<gvr::GvrApi> gvr_api_;

  base::WeakPtrFactory<GvrDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GvrDevice);
};

}  // namespace device

#endif  // DEVICE_VR_ANDROID_GVR_DEVICE_H
