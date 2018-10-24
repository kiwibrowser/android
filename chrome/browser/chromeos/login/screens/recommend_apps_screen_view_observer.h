// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_VIEW_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_VIEW_OBSERVER_H_

#include "base/macros.h"

namespace chromeos {

class RecommendAppsScreenView;

class RecommendAppsScreenViewObserver {
 public:
  virtual ~RecommendAppsScreenViewObserver() = default;

  // Called when the user skips the Recommend Apps screen.
  virtual void OnSkip() = 0;

  // Called when the user tries to reload the screen.
  virtual void OnRetry() = 0;

  // Called when the user Install the selected apps.
  virtual void OnInstall() = 0;

  // Called when the view is destroyed so there is no dead reference to it.
  virtual void OnViewDestroyed(RecommendAppsScreenView* view) = 0;

 protected:
  RecommendAppsScreenViewObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(RecommendAppsScreenViewObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_VIEW_OBSERVER_H_
