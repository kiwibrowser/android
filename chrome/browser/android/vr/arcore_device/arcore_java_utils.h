// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_

#include <jni.h>
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"

namespace device {
class ARCoreDevice;
}

namespace vr {

class ArCoreJavaUtils {
 public:
  static base::android::ScopedJavaLocalRef<jobject> GetApplicationContext();
  static bool EnsureLoaded();
  explicit ArCoreJavaUtils(device::ARCoreDevice* arcore_device);
  ~ArCoreJavaUtils();
  bool ShouldRequestInstallSupportedArCore();
  void RequestInstallSupportedArCore(
      base::android::ScopedJavaLocalRef<jobject> j_tab_android);

  // Method called from the Java side
  void OnRequestInstallSupportedArCoreCanceled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

 private:
  device::ARCoreDevice* arcore_device_;
  base::android::ScopedJavaGlobalRef<jobject> j_arcore_java_utils_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_JAVA_UTILS_H_
