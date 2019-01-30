// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "components/ntp_snippets/remote/remote_suggestions_fetcher_impl.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "jni/PrefetchTestBridge_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// Below is the native implementation of PrefetchTestBridge.java.

namespace offline_pages {
namespace prefetch {

JNI_EXPORT void JNI_PrefetchTestBridge_EnableLimitlessPrefetching(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    jboolean enable) {
  SetLimitlessPrefetchingEnabledForTesting(enable != 0);
}

JNI_EXPORT jboolean JNI_PrefetchTestBridge_IsLimitlessPrefetchingEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  return static_cast<jboolean>(IsLimitlessPrefetchingEnabled());
}

JNI_EXPORT void JNI_PrefetchTestBridge_SkipNTPSuggestionsAPIKeyCheck(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  ntp_snippets::RemoteSuggestionsFetcherImpl::
      set_skip_api_key_check_for_testing();
}

}  // namespace prefetch
}  // namespace offline_pages
