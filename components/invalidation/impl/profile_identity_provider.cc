// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/profile_identity_provider.h"

#include "components/signin/core/browser/profile_oauth2_token_service.h"

namespace invalidation {

ProfileIdentityProvider::ProfileIdentityProvider(
    SigninManagerBase* signin_manager,
    ProfileOAuth2TokenService* token_service)
    : signin_manager_(signin_manager), token_service_(token_service) {
  signin_manager_->AddObserver(this);
}

ProfileIdentityProvider::~ProfileIdentityProvider() {
  // In unittests |signin_manager_| is allowed to be null.
  // TODO(809452): Eliminate this short-circuit when this class is converted to
  // take in IdentityManager, at which point the tests can use
  // IdentityTestEnvironment.
  if (!signin_manager_)
    return;

  signin_manager_->RemoveObserver(this);
}

std::string ProfileIdentityProvider::GetActiveAccountId() {
  // In unittests |signin_manager_| is allowed to be null.
  // TODO(809452): Eliminate this short-circuit when this class is converted to
  // take in IdentityManager, at which point the tests can use
  // IdentityTestEnvironment.
  if (!signin_manager_)
    return std::string();

  return signin_manager_->GetAuthenticatedAccountId();
}

bool ProfileIdentityProvider::IsActiveAccountAvailable() {
  if (GetActiveAccountId().empty() || !token_service_ ||
      !token_service_->RefreshTokenIsAvailable(GetActiveAccountId()))
    return false;

  return true;
}

OAuth2TokenService* ProfileIdentityProvider::GetTokenService() {
  return token_service_;
}

void ProfileIdentityProvider::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username) {
  FireOnActiveAccountLogin();
}

void ProfileIdentityProvider::GoogleSignedOut(const std::string& account_id,
                                              const std::string& username) {
  FireOnActiveAccountLogout();
}

}  // namespace invalidation
