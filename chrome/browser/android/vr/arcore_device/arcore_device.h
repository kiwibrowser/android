// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_

#include <jni.h>
#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/content_settings/core/common/content_settings.h"
#include "device/vr/vr_device.h"
#include "device/vr/vr_device_base.h"

namespace vr {
class MailboxToSurfaceBridge;
class ArCoreJavaUtils;
}  // namespace vr

namespace content {
class WebContents;
}  // namespace content

namespace device {

class ARCoreGlThread;

class ARCoreDevice : public VRDeviceBase {
 public:
  ARCoreDevice();
  ~ARCoreDevice() override;

  // VRDeviceBase implementation.
  void PauseTracking() override;
  void ResumeTracking() override;
  void RequestSession(const XRDeviceRuntimeSessionOptions& options,
                      VRDeviceRequestSessionCallback callback) override;

  base::WeakPtr<ARCoreDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnRequestInstallSupportedARCoreCanceled();

 private:
  // VRDeviceBase implementation
  bool ShouldPauseTrackingWhenFrameDataRestricted() override;
  void OnMagicWindowFrameDataRequest(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback) override;
  void RequestHitTest(
      mojom::XRRayPtr ray,
      mojom::VRMagicWindowProvider::RequestHitTestCallback callback) override;

  void OnMailboxBridgeReady();
  void OnARCoreGlThreadInitialized();
  void OnRequestCameraPermissionComplete(
      base::OnceCallback<void(bool)> callback,
      bool success);

  template <typename... Args>
  static void RunCallbackOnTaskRunner(
      const scoped_refptr<base::TaskRunner>& task_runner,
      base::OnceCallback<void(Args...)> callback,
      Args... args) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::forward<Args>(args)...));
  }
  template <typename... Args>
  base::OnceCallback<void(Args...)> CreateMainThreadCallback(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(&ARCoreDevice::RunCallbackOnTaskRunner<Args...>,
                          main_thread_task_runner_, std::move(callback));
  }

  void PostTaskToGlThread(base::OnceClosure task);

  bool IsOnMainThread();

  void SatisfyRequestSessionPreconditions(
      int render_process_id,
      int render_frame_id,
      bool has_user_activation,
      base::OnceCallback<void(bool)> callback);
  void OnRequestARCoreInstallOrUpdateComplete(
      int render_process_id,
      int render_frame_id,
      bool has_user_activation,
      base::OnceCallback<void(bool)> callback);
  void CallDeferredRequestInstallSupportedARCore();
  void RequestCameraPermission(int render_process_id,
                               int render_frame_id,
                               bool has_user_activation,
                               base::OnceCallback<void(bool)> callback);
  void OnRequestCameraPermissionResult(content::WebContents* web_contents,
                                       base::OnceCallback<void(bool)> callback,
                                       ContentSetting content_setting);
  void OnRequestAndroidCameraPermissionResult(
      base::OnceCallback<void(bool)> callback,
      bool was_android_camera_permission_granted);
  void OnRequestSessionPreconditionsComplete(
      VRDeviceRequestSessionCallback callback,
      bool success);
  void OnARCoreGlInitializationComplete(VRDeviceRequestSessionCallback callback,
                                        bool success);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;
  std::unique_ptr<ARCoreGlThread> arcore_gl_thread_;
  std::unique_ptr<vr::ArCoreJavaUtils> arcore_java_utils_;

  bool is_arcore_gl_thread_initialized_ = false;
  bool is_arcore_gl_initialized_ = false;

  // This object is not paused when it is created. Although it is not
  // necessarily running during initialization, it is not paused. If it is
  // paused before initialization completes, then the underlying runtime will
  // not be resumed.
  bool is_paused_ = false;

  // TODO(https://)
  std::vector<base::OnceCallback<void()>>
      deferred_request_install_supported_arcore_callbacks_;

  // Must be last.
  base::WeakPtrFactory<ARCoreDevice> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ARCoreDevice);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_H_
