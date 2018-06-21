// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_oauth2_token_service.h"

#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#if defined(OS_IOS)
#include "components/signin/core/browser/signin_pref_names.h"
#endif

ProfileOAuth2TokenService::ProfileOAuth2TokenService(
    std::unique_ptr<OAuth2TokenServiceDelegate> delegate)
    : OAuth2TokenService(std::move(delegate)),
      all_credentials_loaded_(false),
      diagnostics_client_(nullptr) {
  AddObserver(this);
}

ProfileOAuth2TokenService::~ProfileOAuth2TokenService() {
  RemoveObserver(this);
}

// static
void ProfileOAuth2TokenService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_IOS)
  registry->RegisterBooleanPref(prefs::kTokenServiceExcludeAllSecondaryAccounts,
                                false);
  registry->RegisterListPref(prefs::kTokenServiceExcludedSecondaryAccounts);
#endif
}

void ProfileOAuth2TokenService::Shutdown() {
  CancelAllRequests();
  GetDelegate()->Shutdown();
}

void ProfileOAuth2TokenService::LoadCredentials(
    const std::string& primary_account_id) {
  GetDelegate()->LoadCredentials(primary_account_id);
}

bool ProfileOAuth2TokenService::AreAllCredentialsLoaded() {
  return all_credentials_loaded_;
}

void ProfileOAuth2TokenService::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  GetDelegate()->UpdateCredentials(account_id, refresh_token);
}

void ProfileOAuth2TokenService::RevokeCredentials(
    const std::string& account_id) {
  GetDelegate()->RevokeCredentials(account_id);
}

const net::BackoffEntry* ProfileOAuth2TokenService::GetDelegateBackoffEntry() {
  return GetDelegate()->BackoffEntry();
}

void ProfileOAuth2TokenService::OnRefreshTokenAvailable(
    const std::string& account_id) {
  // Check if the newly-updated token is valid (invalid tokens are inserted when
  // the user signs out on the web with DICE enabled).
  bool is_valid = true;
  GoogleServiceAuthError token_error = GetAuthError(account_id);
  if (token_error == GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                         GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                             CREDENTIALS_REJECTED_BY_CLIENT)) {
    is_valid = false;
  }

  // NOTE: The code executed in the rest of this method does not affect the
  // state of the accounts in this object, so it doesn't matter whether the
  // callout to |diagnostics_client_| is made before or after. If that fact ever
  // changes, it will be necessary to reason about what the ordering should be.
  if (diagnostics_client_) {
    diagnostics_client_->WillFireOnRefreshTokenAvailable(account_id, is_valid);
  }

  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokenRevoked(
    const std::string& account_id) {
  // NOTE: The code executed in the rest of this method does not affect the
  // state of the accounts in this object, so it doesn't matter whether the
  // callout to |diagnostics_client_| is made before or after. If that fact ever
  // changes, it will be necessary to reason about what the ordering should be.
  if (diagnostics_client_) {
    diagnostics_client_->WillFireOnRefreshTokenRevoked(account_id);
  }

  CancelRequestsForAccount(account_id);
  ClearCacheForAccount(account_id);
}

void ProfileOAuth2TokenService::OnRefreshTokensLoaded() {
  all_credentials_loaded_ = true;
}
