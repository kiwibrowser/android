// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"

#include <jni.h>
#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl_thread.h"
#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"

using base::android::JavaRef;

namespace {
constexpr float kDegreesPerRadian = 180.0f / base::kPiFloat;
}  // namespace

namespace device {

namespace {

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(uint32_t device_id) {
  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();
  device->index = device_id;
  device->displayName = "ARCore VR Device";
  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = true;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = false;
  device->capabilities->can_provide_pass_through_images = true;
  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = nullptr;
  mojom::VREyeParametersPtr& left_eye = device->leftEye;
  left_eye->fieldOfView = mojom::VRFieldOfView::New();
  // TODO(lincolnfrog): get these values for real (see gvr device).
  uint width = 1080;
  uint height = 1795;
  double fov_x = 1437.387;
  double fov_y = 1438.074;
  // TODO(lincolnfrog): get real camera intrinsics.
  float horizontal_degrees = atan(width / (2.0 * fov_x)) * kDegreesPerRadian;
  float vertical_degrees = atan(height / (2.0 * fov_y)) * kDegreesPerRadian;
  left_eye->fieldOfView->leftDegrees = horizontal_degrees;
  left_eye->fieldOfView->rightDegrees = horizontal_degrees;
  left_eye->fieldOfView->upDegrees = vertical_degrees;
  left_eye->fieldOfView->downDegrees = vertical_degrees;
  left_eye->offset = {0.0f, 0.0f, 0.0f};
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  return device;
}

}  // namespace

ARCoreDevice::ARCoreDevice()
    : VRDeviceBase(VRDeviceId::ARCORE_DEVICE_ID),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      mailbox_bridge_(std::make_unique<vr::MailboxToSurfaceBridge>()),
      weak_ptr_factory_(this) {
  SetVRDisplayInfo(CreateVRDisplayInfo(GetId()));

  arcore_java_utils_ = std::make_unique<vr::ArCoreJavaUtils>(this);

  // TODO(https://crbug.com/836524) clean up usage of mailbox bridge
  // and extract the methods in this class that interact with ARCore API
  // into a separate class that implements the ARCore interface.
  mailbox_bridge_->CreateUnboundContextProvider(
      base::BindOnce(&ARCoreDevice::OnMailboxBridgeReady, GetWeakPtr()));
}

ARCoreDevice::~ARCoreDevice() {
}

void ARCoreDevice::PauseTracking() {
  DCHECK(IsOnMainThread());

  if (is_paused_)
    return;

  is_paused_ = true;

  if (!is_arcore_gl_initialized_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::Pause, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr()));
}

void ARCoreDevice::ResumeTracking() {
  DCHECK(IsOnMainThread());

  if (!is_paused_)
    return;

  is_paused_ = false;

  if (!deferred_request_install_supported_arcore_callbacks_.empty())
    CallDeferredRequestInstallSupportedARCore();

  if (!is_arcore_gl_initialized_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::Resume, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr()));
}

void ARCoreDevice::OnMailboxBridgeReady() {
  DCHECK(IsOnMainThread());
  DCHECK(!arcore_gl_thread_);
  // MailboxToSurfaceBridge's destructor's call to DestroyContext must
  // happen on the GL thread, so transferring it to that thread is appropriate.
  // TODO(https://crbug.com/836553): use same GL thread as GVR.
  arcore_gl_thread_ = std::make_unique<ARCoreGlThread>(
      std::move(mailbox_bridge_),
      CreateMainThreadCallback(base::BindOnce(
          &ARCoreDevice::OnARCoreGlThreadInitialized, GetWeakPtr())));
  arcore_gl_thread_->Start();
}

void ARCoreDevice::OnARCoreGlThreadInitialized() {
  DCHECK(IsOnMainThread());

  is_arcore_gl_thread_initialized_ = true;
}

void ARCoreDevice::RequestSession(const XRDeviceRuntimeSessionOptions& options,
                                  VRDeviceRequestSessionCallback callback) {
  DCHECK(IsOnMainThread());

  // TODO(https://crbug.com/849568): Instead of splitting the initialization
  // of this class between construction and RequestSession, perform all the
  // initialization at once on the first successful RequestSession call.

  // TODO(https://crbug.com/846521): If the RequestSession call comes before
  // the arcore gl thread is initialized, the resolution of the request should
  // be delayed.
  if (!is_arcore_gl_thread_initialized_) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  auto preconditions_complete_callback =
      base::BindOnce(&ARCoreDevice::OnRequestSessionPreconditionsComplete,
                     GetWeakPtr(), std::move(callback));

  SatisfyRequestSessionPreconditions(
      options.render_process_id, options.render_frame_id,
      options.has_user_activation, std::move(preconditions_complete_callback));
}

void ARCoreDevice::SatisfyRequestSessionPreconditions(
    int render_process_id,
    int render_frame_id,
    bool has_user_activation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!arcore_java_utils_->ShouldRequestInstallSupportedArCore()) {
    // TODO(https://crbug.com/845792): Consider calling a method to ask for the
    // appropriate permissions.
    // ARCore sessions require camera permission.
    RequestCameraPermission(
        render_process_id, render_frame_id, has_user_activation,
        base::BindOnce(&ARCoreDevice::OnRequestCameraPermissionComplete,
                       GetWeakPtr(), std::move(callback)));
    return;
  }

  // ARCore is not installed or requires an update. Store the callback to be
  // processed later and only the first request session will trigger the
  // request to install or update of the ARCore APK.
  auto deferred_callback =
      base::BindOnce(&ARCoreDevice::OnRequestARCoreInstallOrUpdateComplete,
                     GetWeakPtr(), render_process_id, render_frame_id,
                     has_user_activation, std::move(callback));
  deferred_request_install_supported_arcore_callbacks_.push_back(
      std::move(deferred_callback));
  if (deferred_request_install_supported_arcore_callbacks_.size() > 1)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab_android);

  base::android::ScopedJavaLocalRef<jobject> j_tab_android =
      tab_android->GetJavaObject();
  DCHECK(!j_tab_android.is_null());

  arcore_java_utils_->RequestInstallSupportedArCore(j_tab_android);
}

void ARCoreDevice::OnRequestInstallSupportedARCoreCanceled() {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);
  DCHECK(!deferred_request_install_supported_arcore_callbacks_.empty());

  CallDeferredRequestInstallSupportedARCore();
}

void ARCoreDevice::CallDeferredRequestInstallSupportedARCore() {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);
  DCHECK(!deferred_request_install_supported_arcore_callbacks_.empty());

  for (auto& deferred_callback :
       deferred_request_install_supported_arcore_callbacks_) {
    std::move(deferred_callback).Run();
  }
  deferred_request_install_supported_arcore_callbacks_.clear();
}

void ARCoreDevice::OnRequestARCoreInstallOrUpdateComplete(
    int render_process_id,
    int render_frame_id,
    bool has_user_activation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (arcore_java_utils_->ShouldRequestInstallSupportedArCore()) {
    std::move(callback).Run(false);
    return;
  }

  RequestCameraPermission(
      render_process_id, render_frame_id, has_user_activation,
      base::BindOnce(&ARCoreDevice::OnRequestCameraPermissionComplete,
                     GetWeakPtr(), std::move(callback)));
}

void ARCoreDevice::OnRequestCameraPermissionComplete(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  // By this point ARCore has already been set up, so just return whether the
  // permission request was a success.
  std::move(callback).Run(success);
}

bool ARCoreDevice::ShouldPauseTrackingWhenFrameDataRestricted() {
  return true;
}

void ARCoreDevice::OnMagicWindowFrameDataRequest(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::VRMagicWindowProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsOnMainThread());

  // TODO(ijamardo): Do we need to queue requests to avoid breaking
  // applications?
  // TODO(https://crbug.com/837944): Ensure is_arcore_gl_thread_initialized_
  // is always true by blocking requestDevice()'s callback until it is true
  if (is_paused_ || !is_arcore_gl_thread_initialized_) {
    std::move(callback).Run(nullptr);
    return;
  }

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::ProduceFrame, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr(),
      frame_size, display_rotation,
      CreateMainThreadCallback(std::move(callback))));
}

void ARCoreDevice::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::VRMagicWindowProvider::RequestHitTestCallback callback) {
  DCHECK(IsOnMainThread());

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::RequestHitTest, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr(),
      std::move(ray), CreateMainThreadCallback(std::move(callback))));
}

void ARCoreDevice::PostTaskToGlThread(base::OnceClosure task) {
  DCHECK(IsOnMainThread());
  arcore_gl_thread_->GetARCoreGl()->GetGlThreadTaskRunner()->PostTask(
      FROM_HERE, std::move(task));
}

bool ARCoreDevice::IsOnMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

void ARCoreDevice::RequestCameraPermission(
    int render_process_id,
    int render_frame_id,
    bool has_user_activation,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  // The RFH may have been destroyed by the time the request is processed.
  DCHECK(rfh);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  PermissionManager* permission_manager = PermissionManager::Get(profile);

  permission_manager->RequestPermission(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, rfh, web_contents->GetURL(),
      has_user_activation,
      base::BindRepeating(&ARCoreDevice::OnRequestCameraPermissionResult,
                          GetWeakPtr(), web_contents, base::Passed(&callback)));
}

void ARCoreDevice::OnRequestCameraPermissionResult(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    ContentSetting content_setting) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  // If the camera permission is not allowed, abort the request.
  if (content_setting != CONTENT_SETTING_ALLOW) {
    std::move(callback).Run(false);
    return;
  }

  // Even if the content setting stated that the camera access is allowed,
  // the Android camera permission might still need to be requested, so check
  // if the OS level permission infobar should be shown.
  std::vector<ContentSettingsType> content_settings_types;
  content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  ShowPermissionInfoBarState show_permission_info_bar_state =
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfoBar(
          web_contents, content_settings_types);
  switch (show_permission_info_bar_state) {
    case ShowPermissionInfoBarState::NO_NEED_TO_SHOW_PERMISSION_INFOBAR:
      std::move(callback).Run(true);
      return;
    case ShowPermissionInfoBarState::SHOW_PERMISSION_INFOBAR:
      // Show the Android camera permission info bar.
      PermissionUpdateInfoBarDelegate::Create(
          web_contents, content_settings_types,
          base::BindRepeating(
              &ARCoreDevice::OnRequestAndroidCameraPermissionResult,
              GetWeakPtr(), base::Passed(&callback)));
      return;
    case ShowPermissionInfoBarState::CANNOT_SHOW_PERMISSION_INFOBAR:
      std::move(callback).Run(false);
      return;
  }

  NOTREACHED() << "Unknown show permission infobar state.";
}

void ARCoreDevice::OnRequestSessionPreconditionsComplete(
    VRDeviceRequestSessionCallback callback,
    bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!success) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  if (is_arcore_gl_initialized_) {
    OnARCoreGlInitializationComplete(std::move(callback), true);
    return;
  }

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::Initialize, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr(),
      CreateMainThreadCallback(
          base::BindOnce(&ARCoreDevice::OnARCoreGlInitializationComplete,
                         GetWeakPtr(), std::move(callback)))));
}

void ARCoreDevice::OnARCoreGlInitializationComplete(
    VRDeviceRequestSessionCallback callback,
    bool success) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  if (!success) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  is_arcore_gl_initialized_ = true;

  if (!is_paused_) {
    PostTaskToGlThread(base::BindOnce(
        &ARCoreGl::Resume, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr()));
  }

  // TODO(offenwanger) When the XRMagicWindowProvider or equivalent is returned
  // here, clean out this dummy code.
  auto connection = mojom::XRPresentationConnection::New();
  mojom::VRSubmitFrameClientPtr submit_client;
  connection->client_request = mojo::MakeRequest(&submit_client);
  mojom::VRPresentationProviderPtr provider;
  mojo::MakeRequest(&provider);
  connection->provider = provider.PassInterface();
  connection->transport_options = mojom::VRDisplayFrameTransportOptions::New();
  std::move(callback).Run(std::move(connection), nullptr);
}

void ARCoreDevice::OnRequestAndroidCameraPermissionResult(
    base::OnceCallback<void(bool)> callback,
    bool was_android_camera_permission_granted) {
  DCHECK(IsOnMainThread());
  DCHECK(is_arcore_gl_thread_initialized_);

  std::move(callback).Run(was_android_camera_permission_granted);
}

}  // namespace device
