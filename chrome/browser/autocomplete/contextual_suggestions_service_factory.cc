// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/contextual_suggestions_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/contextual_suggestions_service.h"
#include "content/public/browser/storage_partition.h"

// static
ContextualSuggestionsService*
ContextualSuggestionsServiceFactory::GetForProfile(Profile* profile,
                                                   bool create_if_necessary) {
  return static_cast<ContextualSuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, create_if_necessary));
}

// static
ContextualSuggestionsServiceFactory*
ContextualSuggestionsServiceFactory::GetInstance() {
  return base::Singleton<ContextualSuggestionsServiceFactory>::get();
}

KeyedService* ContextualSuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return new ContextualSuggestionsService(
      identity_manager,
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess());
}

ContextualSuggestionsServiceFactory::ContextualSuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ContextualSuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

ContextualSuggestionsServiceFactory::~ContextualSuggestionsServiceFactory() {}
