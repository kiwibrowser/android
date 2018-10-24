// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_KIOSK_APP_MENU_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_KIOSK_APP_MENU_UPDATER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"

namespace chromeos {

// Observer class to update the Kiosk app menu when Kiosk app data is changed.
class KioskAppMenuUpdater : public KioskAppManagerObserver {
 public:
  KioskAppMenuUpdater();
  ~KioskAppMenuUpdater() override;

  // Manually dispatch kiosk app data to Ash.
  void SendKioskApps();

  // KioskAppManagerObserver:
  void OnKioskAppDataChanged(const std::string& app_id) override;
  void OnKioskAppDataLoadFailure(const std::string& app_id) override;
  void OnKioskAppsSettingsChanged() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskAppMenuUpdater);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_KIOSK_APP_MENU_UPDATER_H_
