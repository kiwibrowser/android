// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen_view_observer.h"

namespace chromeos {

class RecommendAppsScreen;

// Interface for dependency injection between RecommendAppsScreen and its
// WebUI representation.
class RecommendAppsScreenView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_RECOMMEND_APPS;

  virtual ~RecommendAppsScreenView() = default;

  // Adds/Removes observer for view.
  virtual void AddObserver(RecommendAppsScreenViewObserver* observer) = 0;
  virtual void RemoveObserver(RecommendAppsScreenViewObserver* observer) = 0;

  // Sets screen this view belongs to.
  virtual void Bind(RecommendAppsScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Called when the download of the recommend app list fails. Show an error
  // message to the user.
  virtual void OnLoadError() = 0;

  // Called when the download of the recommend app list is successful. Shows the
  // downloaded |app_list| to the user.
  virtual void OnLoadSuccess(const std::string& app_list) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_VIEW_H_
