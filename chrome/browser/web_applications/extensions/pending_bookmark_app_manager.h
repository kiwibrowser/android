// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

namespace extensions {

// Implementation of WebAppPolicyManager::PendingAppManager that manages the set
// of Bookmark Apps which are being installed, uninstalled, and updated.
//
// WebAppPolicyManager creates an instance of this class and manages its
// lifetime. This class should only be used from the UI thread.
class PendingBookmarkAppManager final
    : public web_app::WebAppPolicyManager::PendingAppManager {
 public:
  PendingBookmarkAppManager();
  ~PendingBookmarkAppManager() override;

  // WebAppPolicyManager::PendingAppManager
  void ProcessAppOperations(std::vector<web_app::WebAppPolicyManager::AppInfo>
                                apps_to_install) override;

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_PENDING_BOOKMARK_APP_MANAGER_H_
