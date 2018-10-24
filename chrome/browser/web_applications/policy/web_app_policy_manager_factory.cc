// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace web_app {

// static
WebAppPolicyManager* WebAppPolicyManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<WebAppPolicyManager*>(
      WebAppPolicyManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
WebAppPolicyManagerFactory* WebAppPolicyManagerFactory::GetInstance() {
  return base::Singleton<WebAppPolicyManagerFactory>::get();
}

WebAppPolicyManagerFactory::WebAppPolicyManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAppPolicyManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

WebAppPolicyManagerFactory::~WebAppPolicyManagerFactory() = default;

KeyedService* WebAppPolicyManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new WebAppPolicyManager(profile->GetPrefs());
}

bool WebAppPolicyManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  //  namespace web_app
