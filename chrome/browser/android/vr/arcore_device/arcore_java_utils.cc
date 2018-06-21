// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"
#include "chrome/browser/android/vr/arcore_device/arcore_shim.h"
#include "chrome/browser/vr/service/vr_device_manager.h"
#include "jni/ArCoreJavaUtils_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace vr {

ScopedJavaLocalRef<jobject> ArCoreJavaUtils::GetApplicationContext() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreJavaUtils_getApplicationContext(env);
}

bool ArCoreJavaUtils::EnsureLoaded() {
  JNIEnv* env = AttachCurrentThread();
  if (!Java_ArCoreJavaUtils_shouldLoadArCoreSdk(env))
    return false;

  return LoadArCoreSdk();
}

ArCoreJavaUtils::ArCoreJavaUtils(device::ARCoreDevice* arcore_device)
    : arcore_device_(arcore_device) {
  DCHECK(arcore_device_);

  JNIEnv* env = AttachCurrentThread();
  if (!env)
    return;
  ScopedJavaLocalRef<jobject> j_arcore_java_utils =
      Java_ArCoreJavaUtils_create(env, (jlong)this);
  if (j_arcore_java_utils.is_null())
    return;
  j_arcore_java_utils_.Reset(j_arcore_java_utils);
}

ArCoreJavaUtils::~ArCoreJavaUtils() {
  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreJavaUtils_onNativeDestroy(env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::OnRequestInstallSupportedArCoreCanceled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  arcore_device_->OnRequestInstallSupportedARCoreCanceled();
}

bool ArCoreJavaUtils::ShouldRequestInstallSupportedArCore() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ArCoreJavaUtils_shouldRequestInstallSupportedArCore(
      env, j_arcore_java_utils_);
}

void ArCoreJavaUtils::RequestInstallSupportedArCore(
    base::android::ScopedJavaLocalRef<jobject> j_tab_android) {
  DCHECK(ShouldRequestInstallSupportedArCore());
  JNIEnv* env = AttachCurrentThread();
  Java_ArCoreJavaUtils_requestInstallSupportedArCore(env, j_arcore_java_utils_,
                                                     j_tab_android);
}

}  // namespace vr
