// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

class WebAppPolicyManager;

// Singleton that owns all WebAppPolicyManagerFactories and associates them with
// Profile.
class WebAppPolicyManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebAppPolicyManager* GetForProfile(Profile* profile);

  static WebAppPolicyManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WebAppPolicyManagerFactory>;

  WebAppPolicyManagerFactory();
  ~WebAppPolicyManagerFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(WebAppPolicyManagerFactory);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_POLICY_WEB_APP_POLICY_MANAGER_FACTORY_H_
