// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/consent_auditor/consent_auditor_factory.h"

#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/consent_auditor/consent_sync_bridge_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/version_info/version_info.h"

// static
ConsentAuditorFactory* ConsentAuditorFactory::GetInstance() {
  return base::Singleton<ConsentAuditorFactory>::get();
}

// static
consent_auditor::ConsentAuditor* ConsentAuditorFactory::GetForProfile(
    Profile* profile) {
  // Recording local consents in Incognito is not useful, as the record would
  // soon disappear. Consents tied to the user's Google account should retrieve
  // account information from the original profile. In both cases, there is no
  // reason to support Incognito.
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<consent_auditor::ConsentAuditor*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

ConsentAuditorFactory::ConsentAuditorFactory()
    : BrowserContextKeyedServiceFactory(
          "ConsentAuditor",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(browser_sync::UserEventServiceFactory::GetInstance());
  // TODO(crbug.com/850428): This is missing
  // DependsOn(ProfileSyncServiceFactory::GetInstance()), which we can't
  // simply add because ProfileSyncServiceFactory itself depends on this
  // factory.
}

ConsentAuditorFactory::~ConsentAuditorFactory() {}

KeyedService* ConsentAuditorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  std::unique_ptr<syncer::ConsentSyncBridge> bridge;
  if (base::FeatureList::IsEnabled(switches::kSyncUserConsentSeparateType)) {
    syncer::OnceModelTypeStoreFactory store_factory =
        browser_sync::ProfileSyncService::GetModelTypeStoreFactory(
            profile->GetPath());
    auto change_processor =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::USER_CONSENTS,
            base::BindRepeating(&syncer::ReportUnrecoverableError,
                                chrome::GetChannel()));
    syncer::SyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile);
    bridge = std::make_unique<syncer::ConsentSyncBridgeImpl>(
        std::move(store_factory), std::move(change_processor),
        /*authenticated_account_id_callback=*/
        base::BindRepeating(
            [](syncer::SyncService* sync_service) {
              return sync_service->GetAuthenticatedAccountInfo().account_id;
            },
            base::Unretained(sync_service)));
  }
  // TODO(vitaliii): Don't create UserEventService when it won't be used.
  return new consent_auditor::ConsentAuditor(
      profile->GetPrefs(), std::move(bridge),
      browser_sync::UserEventServiceFactory::GetForProfile(profile),
      // The browser version and locale do not change runtime, so we can pass
      // them directly.
      version_info::GetVersionNumber(),
      g_browser_process->GetApplicationLocale());
}

// static
void ConsentAuditorFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  consent_auditor::ConsentAuditor::RegisterProfilePrefs(registry);
}
