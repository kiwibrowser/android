// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_IDENTITY_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_IDENTITY_PROVIDER_H_

#include "base/macros.h"
#include "components/invalidation/public/identity_provider.h"

namespace chromeos {

class DeviceOAuth2TokenService;

// Identity provider implementation backed by DeviceOAuth2TokenService.
class DeviceIdentityProvider : public invalidation::IdentityProvider {
 public:
  explicit DeviceIdentityProvider(
      chromeos::DeviceOAuth2TokenService* token_service);
  ~DeviceIdentityProvider() override;

  // IdentityProvider:
  std::string GetActiveAccountId() override;
  bool IsActiveAccountAvailable() override;
  OAuth2TokenService* GetTokenService() override;

 private:
  chromeos::DeviceOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(DeviceIdentityProvider);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_IDENTITY_PROVIDER_H_
