// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "services/identity/public/cpp/identity_manager.h"

// Subclass that wraps IdentityManager in a KeyedService (as IdentityManager is
// a client-side library intended for use by any process, it would be a layering
// violation for IdentityManager itself to have direct knowledge of
// KeyedService).
// NOTE: Do not add any code here that further ties IdentityManager to Profile
// without communicating with {blundell, sdefresne}@chromium.org.
class IdentityManagerWrapper : public KeyedService,
                               public identity::IdentityManager {
 public:
  explicit IdentityManagerWrapper(Profile* profile)
      : identity::IdentityManager(
            SigninManagerFactory::GetForProfile(profile),
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
            AccountTrackerServiceFactory::GetForProfile(profile)) {}
};

IdentityManagerFactory::IdentityManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "IdentityManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AccountTrackerServiceFactory::GetInstance());
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
}

IdentityManagerFactory::~IdentityManagerFactory() {}

// static
identity::IdentityManager* IdentityManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
identity::IdentityManager* IdentityManagerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<IdentityManagerWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
IdentityManagerFactory* IdentityManagerFactory::GetInstance() {
  return base::Singleton<IdentityManagerFactory>::get();
}

KeyedService* IdentityManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new IdentityManagerWrapper(Profile::FromBrowserContext(context));
}
