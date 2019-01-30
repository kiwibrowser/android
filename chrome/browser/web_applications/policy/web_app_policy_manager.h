// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class PrefService;
class GURL;

namespace web_app {

// Tracks the policy that affects Web Apps and also tracks which Web Apps are
// currently installed based on this policy. Based on these, it decides
// which apps need to be installed, uninstalled, and updated. It uses
// WebAppPolicyManager::PendingAppManager to actually install, uninstall,
// and update apps.
class WebAppPolicyManager : public KeyedService {
 public:
  class PendingAppManager;

  // How the app will be launched after installation.
  enum class LaunchType {
    kTab,
    kWindow,
  };

  struct AppInfo {
    AppInfo(GURL url, LaunchType launch_type);
    AppInfo(AppInfo&& other);
    ~AppInfo();

    bool operator==(const AppInfo& other) const;

    GURL url;
    LaunchType launch_type;

    DISALLOW_COPY_AND_ASSIGN(AppInfo);
  };

  // Constructs a WebAppPolicyManager instance that uses
  // extensions::PendingBookmarkAppManager to manage apps.
  explicit WebAppPolicyManager(PrefService* pref_service);

  // Constructs a WebAppPolicyManager instance that uses |pending_app_manager|
  // to manage apps.
  explicit WebAppPolicyManager(
      PrefService* pref_service,
      std::unique_ptr<PendingAppManager> pending_app_manager);

  ~WebAppPolicyManager() override;

  const PendingAppManager& pending_app_manager() {
    return *pending_app_manager_;
  }

 private:
  std::vector<AppInfo> GetAppsToInstall();

  PrefService* pref_service_;
  std::unique_ptr<PendingAppManager> pending_app_manager_;

  DISALLOW_COPY_AND_ASSIGN(WebAppPolicyManager);
};

// Used by WebAppPolicyManager to install, uninstall, and update apps.
//
// Implementations of this class should perform each set of operations serially
// in the order in which they arrive. For example, if an uninstall request gets
// queued while an update request for the same app is pending, implementations
// should wait for the update request to finish before uninstalling the app.
class WebAppPolicyManager::PendingAppManager {
 public:
  PendingAppManager();
  virtual ~PendingAppManager();

  // Starts the installation of |apps_to_install|.
  virtual void ProcessAppOperations(std::vector<AppInfo> apps_to_install) = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingAppManager);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_H_
