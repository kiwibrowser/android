// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <vector>

namespace extensions {

PendingBookmarkAppManager::PendingBookmarkAppManager() = default;

PendingBookmarkAppManager::~PendingBookmarkAppManager() = default;

void PendingBookmarkAppManager::ProcessAppOperations(
    std::vector<web_app::WebAppPolicyManager::AppInfo> apps_to_install) {}

}  // namespace extensions
