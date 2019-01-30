// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_FACTORY_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_FACTORY_H_

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/browser/resource_coordinator/site_characteristics_data_store.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace resource_coordinator {

// Singleton that owns all the LocalSiteCharacteristicsDataStore instances and
// associates them with Profiles.
class LocalSiteCharacteristicsDataStoreFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SiteCharacteristicsDataStore* GetForProfile(Profile* profile);
  static LocalSiteCharacteristicsDataStoreFactory* GetInstance();

  // In production, an instance is created with the profile. In unit tests, no
  // instance is created by default. If this method is called, an instance will
  // be created the first time GetInstance() is called. In most unit tests, a
  // custom factory should be set before the first call to GetInstance().
  static void EnableForTesting();

 private:
  friend class base::NoDestructor<LocalSiteCharacteristicsDataStoreFactory>;

  LocalSiteCharacteristicsDataStoreFactory();
  ~LocalSiteCharacteristicsDataStoreFactory() override;

  // Returns the |SiteCharacteristicsDataStore| instance associated with
  // |context|. This is basically a wrapper around GetServiceForBrowserContext
  // that allows calling it from a const function.
  SiteCharacteristicsDataStore* GetExistingDataStoreForContext(
      content::BrowserContext* context) const;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_DATA_STORE_FACTORY_H_
