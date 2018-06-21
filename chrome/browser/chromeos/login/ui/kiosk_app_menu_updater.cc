// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/kiosk_app_menu_updater.h"

#include "ash/public/interfaces/kiosk_app_info.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

KioskAppMenuUpdater::KioskAppMenuUpdater() {
  KioskAppManager::Get()->AddObserver(this);
}

KioskAppMenuUpdater::~KioskAppMenuUpdater() {
  KioskAppManager::Get()->RemoveObserver(this);
}

void KioskAppMenuUpdater::OnKioskAppDataChanged(const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuUpdater::OnKioskAppDataLoadFailure(const std::string& app_id) {
  SendKioskApps();
}

void KioskAppMenuUpdater::OnKioskAppsSettingsChanged() {
  SendKioskApps();
}

void KioskAppMenuUpdater::SendKioskApps() {
  if (!LoginScreenClient::HasInstance())
    return;

  std::vector<ash::mojom::KioskAppInfoPtr> output;
  std::vector<chromeos::KioskAppManager::App> apps;

  chromeos::KioskAppManager::Get()->GetApps(&apps);
  for (const auto& app : apps) {
    auto mojo_app = ash::mojom::KioskAppInfo::New();
    mojo_app->app_id = app.app_id;
    mojo_app->name = base::UTF8ToUTF16(app.name);
    if (app.icon.isNull()) {
      mojo_app->icon = *ui::ResourceBundle::GetSharedInstance()
                            .GetImageNamed(IDR_APP_DEFAULT_ICON)
                            .ToImageSkia();
    } else {
      mojo_app->icon = gfx::ImageSkia(app.icon);
    }
    output.push_back(std::move(mojo_app));
  }

  LoginScreenClient::Get()->login_screen()->SetKioskApps(std::move(output));
}

}  // namespace chromeos
