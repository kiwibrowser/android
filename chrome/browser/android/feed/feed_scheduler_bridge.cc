// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/feed_scheduler_bridge.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/feed/core/feed_host_service.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "jni/FeedSchedulerBridge_jni.h"

using base::android::JavaRef;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;

namespace feed {

static jlong JNI_FeedSchedulerBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_this,
    const JavaParamRef<jobject>& j_profile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  FeedHostService* host_service =
      FeedHostServiceFactory::GetForBrowserContext(profile);
  return reinterpret_cast<intptr_t>(
      new FeedSchedulerBridge(j_this, host_service->GetSchedulerHost()));
}

FeedSchedulerBridge::FeedSchedulerBridge(const JavaRef<jobject>& j_this,
                                         FeedSchedulerHost* scheduler_host)
    : j_this_(ScopedJavaGlobalRef<jobject>(j_this)),
      scheduler_host_(scheduler_host),
      weak_factory_(this) {
  DCHECK(scheduler_host_);
  scheduler_host_->RegisterTriggerRefreshCallback(base::BindRepeating(
      &FeedSchedulerBridge::TriggerRefresh, weak_factory_.GetWeakPtr()));
}

FeedSchedulerBridge::~FeedSchedulerBridge() {}

void FeedSchedulerBridge::Destroy(JNIEnv* env, const JavaRef<jobject>& j_this) {
  delete this;
}

jint FeedSchedulerBridge::ShouldSessionRequestData(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const jboolean j_has_content,
    const jlong j_content_creation_date_time_ms,
    const jboolean j_has_outstanding_request) {
  return static_cast<int>(scheduler_host_->ShouldSessionRequestData(
      j_has_content, base::Time::FromJavaTime(j_content_creation_date_time_ms),
      j_has_outstanding_request));
}

void FeedSchedulerBridge::OnReceiveNewContent(
    JNIEnv* env,
    const JavaRef<jobject>& j_this,
    const jlong j_content_creation_date_time_ms) {
  scheduler_host_->OnReceiveNewContent(
      base::Time::FromJavaTime(j_content_creation_date_time_ms));
}

void FeedSchedulerBridge::OnRequestError(JNIEnv* env,
                                         const JavaRef<jobject>& j_this,
                                         const jint j_network_response_code) {
  scheduler_host_->OnRequestError(j_network_response_code);
}

void FeedSchedulerBridge::OnForegrounded(JNIEnv* env,
                                         const JavaRef<jobject>& j_this) {
  scheduler_host_->OnForegrounded();
}

void FeedSchedulerBridge::OnFixedTimer(JNIEnv* env,
                                       const JavaRef<jobject>& j_this) {
  scheduler_host_->OnFixedTimer();
}

void FeedSchedulerBridge::TriggerRefresh() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FeedSchedulerBridge_triggerRefresh(env, j_this_);
}

}  // namespace feed
