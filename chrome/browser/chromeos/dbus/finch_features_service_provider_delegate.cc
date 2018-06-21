// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dbus/finch_features_service_provider_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace chromeos {

FinchFeaturesServiceProviderDelegate::FinchFeaturesServiceProviderDelegate() {}

FinchFeaturesServiceProviderDelegate::~FinchFeaturesServiceProviderDelegate() {}

bool FinchFeaturesServiceProviderDelegate::IsCrostiniEnabled(
    const std::string& user_id_hash) {
  Profile* profile = nullptr;
  if (!user_id_hash.empty()) {
    profile = g_browser_process->profile_manager()->GetProfileByPath(
        ProfileHelper::GetProfilePathByUserIdHash(user_id_hash));
  } else {
    profile = ProfileManager::GetActiveUserProfile();
  }

  return IsCrostiniAllowedForProfile(profile);
}

}  // namespace chromeos
