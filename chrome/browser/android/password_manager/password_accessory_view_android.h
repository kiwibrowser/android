// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_MANAGER_PASSWORD_ACCESSORY_VIEW_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_MANAGER_PASSWORD_ACCESSORY_VIEW_ANDROID_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/password_accessory_view_interface.h"

class PasswordAccessoryController;

// This Android-specific implementation of the |PasswordAccessoryViewInterface|
// is the native counterpart of the |PasswordAccessoryViewBridge| java class.
// It's owned by a PasswordAccessoryController which is bound to an activity.
class PasswordAccessoryViewAndroid : public PasswordAccessoryViewInterface {
 public:
  // Builds the UI for the |controller|.
  explicit PasswordAccessoryViewAndroid(
      PasswordAccessoryController* controller);
  ~PasswordAccessoryViewAndroid() override;

  // PasswordAccessoryViewInterface:
  void OnItemsAvailable(const GURL& origin,
                        const std::vector<AccessoryItem>& items) override;

  // Called from Java via JNI:
  void OnFillingTriggered(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& textToFill);

 private:
  // The controller provides data for this view and owns it.
  PasswordAccessoryController* controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(PasswordAccessoryViewAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_MANAGER_PASSWORD_ACCESSORY_VIEW_ANDROID_H_
