// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen_view.h"

namespace network {
class SimpleURLLoader;
}

namespace chromeos {

class BaseScreenDelegate;

// This is Recommend Apps screen that is displayed as a part of user first
// sign-in flow.
class RecommendAppsScreen : public BaseScreen,
                            public RecommendAppsScreenViewObserver {
 public:
  RecommendAppsScreen(BaseScreenDelegate* base_screen_delegate,
                      RecommendAppsScreenView* view);
  ~RecommendAppsScreen() override;

  // BaseScreen:
  void Show() override;
  void Hide() override;

  // RecommendAppsScreenViewObserver:
  void OnSkip() override;
  void OnRetry() override;
  void OnInstall() override;
  void OnViewDestroyed(RecommendAppsScreenView* view) override;

 private:
  // Start downloading the recommended app list.
  void StartDownload();

  // Abort the attempt to download the recommended app list if it takes too
  // long.
  void OnDownloadTimeout();

  // Callback function called when SimpleURLLoader completes.
  void OnDownloaded(std::unique_ptr<std::string> response_body);

  RecommendAppsScreenView* view_;

  std::unique_ptr<network::SimpleURLLoader> app_list_loader_;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the recommended app list.
  base::OneShotTimer download_timer_;

  DISALLOW_COPY_AND_ASSIGN(RecommendAppsScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_SCREEN_H_
