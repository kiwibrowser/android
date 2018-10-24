// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefChangeRegistrar;
class PrefService;

namespace browser_sync {
class ProfileSyncService;
}

class UnifiedConsentServiceClient;

// A browser-context keyed service that is used to manage the user consent
// when UnifiedConsent feature is enabled.
class UnifiedConsentService : public KeyedService,
                              public identity::IdentityManager::Observer {
 public:
  //
  UnifiedConsentService(UnifiedConsentServiceClient* service_client,
                        PrefService* pref_service,
                        identity::IdentityManager* identity_manager,
                        browser_sync::ProfileSyncService* profile_sync_service);
  ~UnifiedConsentService() override;

  // Register the prefs used by this UnifiedConsentService.
  static void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry);

  // KeyedService:
  void Shutdown() override;

  // IdentityManager::Observer:
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;

 private:
  // Called when |prefs::kUnifiedConsentGiven| pref value changes.
  // When set to true, it enables syncing of all data types and it enables all
  // non-personalized services. Otherwise it does nothing.
  void OnUnifiedConsentGivenPrefChanged();

  UnifiedConsentServiceClient* service_client_;
  PrefService* pref_service_;
  identity::IdentityManager* identity_manager_;
  browser_sync::ProfileSyncService* profile_sync_service_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedConsentService);
};

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_SERVICE_H_
