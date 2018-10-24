// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/invalidation/public/identity_provider.h"
#include "components/signin/core/browser/signin_manager_base.h"

class ProfileOAuth2TokenService;

namespace invalidation {

// An identity provider implementation that's backed by
// ProfileOAuth2TokenService and SigninManager.
class ProfileIdentityProvider : public IdentityProvider,
                                public SigninManagerBase::Observer {
 public:
  ProfileIdentityProvider(SigninManagerBase* signin_manager,
                          ProfileOAuth2TokenService* token_service);
#if defined(UNIT_TEST)
  // Provide a testing constructor that allows for a null SigninManager instance
  // to be passed, as there are tests that don't interact with the login
  // functionality and it is a pain to set up FakeSigninManager(Base).
  // TODO(809452): Eliminate this testing constructor when this class is
  // converted to take in IdentityManager, at which point the tests can use
  // IdentityTestEnvironment.
  ProfileIdentityProvider(ProfileOAuth2TokenService* token_service)
      : signin_manager_(nullptr), token_service_(token_service) {}
#endif
  ~ProfileIdentityProvider() override;

  // IdentityProvider:
  std::string GetActiveAccountId() override;
  bool IsActiveAccountAvailable() override;
  OAuth2TokenService* GetTokenService() override;

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

 private:
  SigninManagerBase* const signin_manager_;
  ProfileOAuth2TokenService* const token_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIdentityProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PROFILE_IDENTITY_PROVIDER_H_
