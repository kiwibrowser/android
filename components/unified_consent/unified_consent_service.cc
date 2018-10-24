// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_service.h"

#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/sync/base/model_type.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service_client.h"

UnifiedConsentService::UnifiedConsentService(
    UnifiedConsentServiceClient* service_client,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    browser_sync::ProfileSyncService* profile_sync_service)
    : service_client_(service_client),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      profile_sync_service_(profile_sync_service) {
  DCHECK(pref_service_);
  DCHECK(identity_manager_);
  DCHECK(profile_sync_service_);
  identity_manager_->AddObserver(this);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUnifiedConsentGiven,
      base::BindRepeating(
          &UnifiedConsentService::OnUnifiedConsentGivenPrefChanged,
          base::Unretained(this)));
}

UnifiedConsentService::~UnifiedConsentService() {}

void UnifiedConsentService::RegisterPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kUnifiedConsentGiven, false);
}

void UnifiedConsentService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void UnifiedConsentService::OnPrimaryAccountCleared(
    const AccountInfo& account_info) {
  // By design, signing out of Chrome automatically disables the user consent
  // for making search and browsing better.
  pref_service_->SetBoolean(prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
                            false);
  // When signing out, the unfied consent is revoked.
  pref_service_->SetBoolean(prefs::kUnifiedConsentGiven, false);
}

void UnifiedConsentService::OnUnifiedConsentGivenPrefChanged() {
  bool enabled = pref_service_->GetBoolean(prefs::kUnifiedConsentGiven);

  if (!enabled) {
    if (identity_manager_->HasPrimaryAccount()) {
      // KeepEverythingSynced is set to false, so the user can select individual
      // sync data types.
      profile_sync_service_->OnUserChoseDatatypes(
          false, syncer::UserSelectableTypes());
    }
    return;
  }

  DCHECK(profile_sync_service_->IsSyncAllowed());
  DCHECK(identity_manager_->HasPrimaryAccount());

  // Enable all sync data types.
  pref_service_->SetBoolean(autofill::prefs::kAutofillWalletImportEnabled,
                            true);
  profile_sync_service_->OnUserChoseDatatypes(true,
                                              syncer::UserSelectableTypes());

  // Enable all non-personalized services.
  pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  service_client_->SetAlternateErrorPagesEnabled(true);
  service_client_->SetMetricsReportingEnabled(true);
  service_client_->SetSafeBrowsingExtendedReportingEnabled(true);
  service_client_->SetSearchSuggestEnabled(true);
  service_client_->SetSafeBrowsingExtendedReportingEnabled(true);
  service_client_->SetNetworkPredictionEnabled(true);
}
