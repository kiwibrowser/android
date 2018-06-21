// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/explore_sites/ntp_json_fetcher.h"

namespace explore_sites {

/**
 * Bridge between C++ and Java for fetching and decoding URLs and images.
 */
static void JNI_ExploreSitesBridge_GetNtpCategories(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& j_caller,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jobject>& j_result_obj,
    const base::android::JavaParamRef<jobject>& j_callback_obj);

static void JNI_ExploreSitesBridge_GetIcon(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& j_caller,
    const base::android::JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jstring>& j_url,
    const base::android::JavaParamRef<jobject>& j_callback_obj);

static base::android::ScopedJavaLocalRef<jstring>
JNI_ExploreSitesBridge_GetCatalogUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller);

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_BRIDGE_H_
