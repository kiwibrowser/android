// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_manager.h"

#include "google_apis/gaia/gaia_auth_util.h"

namespace identity {

IdentityManager::IdentityManager(SigninManagerBase* signin_manager,
                                 ProfileOAuth2TokenService* token_service,
                                 AccountTrackerService* account_tracker_service)
    : signin_manager_(signin_manager),
      token_service_(token_service),
      account_tracker_service_(account_tracker_service) {
  primary_account_info_ = signin_manager_->GetAuthenticatedAccountInfo();
  signin_manager_->AddObserver(this);
#if !defined(OS_CHROMEOS)
  SigninManager::FromSigninManagerBase(signin_manager_)
      ->set_diagnostics_client(this);
#endif
  token_service_->AddDiagnosticsObserver(this);
  token_service_->set_diagnostics_client(this);
}

IdentityManager::~IdentityManager() {
  signin_manager_->RemoveObserver(this);
#if !defined(OS_CHROMEOS)
  SigninManager::FromSigninManagerBase(signin_manager_)
      ->set_diagnostics_client(nullptr);
#endif
  token_service_->RemoveDiagnosticsObserver(this);
  token_service_->set_diagnostics_client(nullptr);
}

AccountInfo IdentityManager::GetPrimaryAccountInfo() {
#if defined(OS_CHROMEOS)
  // On ChromeOS in production, the authenticated account is set very early in
  // startup and never changed. Hence, the information held by the
  // IdentityManager should always correspond to that held by SigninManager.
  // NOTE: the above invariant is not guaranteed to hold in tests. If you
  // are seeing this DCHECK go off in a testing context, it means that you need
  // to set the IdentityManager's primary account info in the test at the place
  // where you are setting the authenticated account info in the SigninManager.
  // TODO(blundell): Add the API to do this once we hit the first case and
  // document the API to use here.
  DCHECK_EQ(signin_manager_->GetAuthenticatedAccountId(),
            primary_account_info_.account_id);

  // Note: If the primary account's refresh token gets revoked, then the account
  // gets removed from AccountTrackerService (via
  // AccountFetcherService::OnRefreshTokenRevoked), and so SigninManager's
  // GetAuthenticatedAccountInfo is empty (even though
  // GetAuthenticatedAccountId is NOT empty).
  if (!signin_manager_->GetAuthenticatedAccountInfo().account_id.empty()) {
    DCHECK_EQ(signin_manager_->GetAuthenticatedAccountInfo().account_id,
              primary_account_info_.account_id);
    DCHECK_EQ(signin_manager_->GetAuthenticatedAccountInfo().gaia,
              primary_account_info_.gaia);

    // TODO(842670): As described in the bug, AccountTrackerService's email
    // address can be updated after it is initially set on ChromeOS. Figure out
    // right long-term solution for this problem.
    if (signin_manager_->GetAuthenticatedAccountInfo().email !=
        primary_account_info_.email) {
      // This update should only be to move it from normalized form to the form
      // in which the user entered the email when creating the account. The
      // below check verifies that the normalized forms of the two email
      // addresses are identical.
      DCHECK(gaia::AreEmailsSame(
          signin_manager_->GetAuthenticatedAccountInfo().email,
          primary_account_info_.email));
      primary_account_info_.email =
          signin_manager_->GetAuthenticatedAccountInfo().email;
    }
  }
#endif  // defined(OS_CHROMEOS)
  return primary_account_info_;
}

bool IdentityManager::HasPrimaryAccount() {
  return !primary_account_info_.account_id.empty();
}

std::unique_ptr<AccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForAccount(
    const std::string& account_id,
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback) {
  return std::make_unique<AccessTokenFetcher>(account_id, oauth_consumer_name,
                                              token_service_, scopes,
                                              std::move(callback));
}

std::unique_ptr<PrimaryAccountAccessTokenFetcher>
IdentityManager::CreateAccessTokenFetcherForPrimaryAccount(
    const std::string& oauth_consumer_name,
    const OAuth2TokenService::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    PrimaryAccountAccessTokenFetcher::Mode mode) {
  return std::make_unique<PrimaryAccountAccessTokenFetcher>(
      oauth_consumer_name, signin_manager_, token_service_, scopes,
      std::move(callback), mode);
}

void IdentityManager::RemoveAccessTokenFromCache(
    const AccountInfo& account_info,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  // TODO(843510): Consider making the request to ProfileOAuth2TokenService
  // asynchronously once there are no direct clients of PO2TS. This change would
  // need to be made together with changing all callsites to
  // ProfileOAuth2TokenService::RequestAccessToken() to be made asynchronously
  // as well (to maintain ordering in the case where a client removes an access
  // token from the cache and then immediately requests an access token).
  token_service_->InvalidateAccessToken(account_info.account_id, scopes,
                                        access_token);
}

void IdentityManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void IdentityManager::AddDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.AddObserver(observer);
}

void IdentityManager::RemoveDiagnosticsObserver(DiagnosticsObserver* observer) {
  diagnostics_observer_list_.RemoveObserver(observer);
}

void IdentityManager::SetPrimaryAccountSynchronouslyForTests(
    const std::string& gaia_id,
    const std::string& email_address,
    const std::string& refresh_token) {
  DCHECK(!refresh_token.empty());
  SetPrimaryAccountSynchronously(gaia_id, email_address, refresh_token);
}

void IdentityManager::SetPrimaryAccountSynchronously(
    const std::string& gaia_id,
    const std::string& email_address,
    const std::string& refresh_token) {
  signin_manager_->SetAuthenticatedAccountInfo(gaia_id, email_address);
  primary_account_info_ = signin_manager_->GetAuthenticatedAccountInfo();

  if (!refresh_token.empty()) {
    token_service_->UpdateCredentials(primary_account_info_.account_id,
                                      refresh_token);
  }
}

#if !defined(OS_CHROMEOS)
void IdentityManager::WillFireGoogleSigninSucceeded(
    const AccountInfo& account_info) {
  // TODO(843510): Consider setting this info and notifying observers
  // asynchronously in response to GoogleSigninSucceeded() once there are no
  // direct clients of SigninManager.
  primary_account_info_ = account_info;
}

void IdentityManager::WillFireGoogleSignedOut(const AccountInfo& account_info) {
  // TODO(843510): Consider setting this info and notifying observers
  // asynchronously in response to GoogleSigninSucceeded() once there are no
  // direct clients of SigninManager.
  DCHECK(account_info.account_id == primary_account_info_.account_id);
  DCHECK(account_info.gaia == primary_account_info_.gaia);
  DCHECK(account_info.email == primary_account_info_.email);
  primary_account_info_ = AccountInfo();
}
#endif

void IdentityManager::GoogleSigninSucceeded(const AccountInfo& account_info) {
  DCHECK(account_info.account_id == primary_account_info_.account_id);
  DCHECK(account_info.gaia == primary_account_info_.gaia);
  DCHECK(account_info.email == primary_account_info_.email);
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountSet(account_info);
  }
}

void IdentityManager::GoogleSignedOut(const AccountInfo& account_info) {
  DCHECK(!HasPrimaryAccount());
  for (auto& observer : observer_list_) {
    observer.OnPrimaryAccountCleared(account_info);
  }
}

void IdentityManager::WillFireOnRefreshTokenAvailable(
    const std::string& account_id,
    bool is_valid) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.IsEmpty());
  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenUpdatedForAccount(account_info, is_valid);
  }
}

void IdentityManager::WillFireOnRefreshTokenRevoked(
    const std::string& account_id) {
  AccountInfo account_info =
      account_tracker_service_->GetAccountInfo(account_id);
  DCHECK(!account_info.IsEmpty());
  for (auto& observer : observer_list_) {
    observer.OnRefreshTokenRemovedForAccount(account_info);
  }
}

void IdentityManager::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  // TODO(843510): Consider notifying observers asynchronously once there
  // are no direct clients of ProfileOAuth2TokenService.
  for (auto& observer : diagnostics_observer_list_) {
    observer.OnAccessTokenRequested(account_id, consumer_id, scopes);
  }
}

}  // namespace identity
