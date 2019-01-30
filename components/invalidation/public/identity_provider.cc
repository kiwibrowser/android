// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/identity_provider.h"

namespace invalidation {

class ActiveAccountAccessTokenFetcherImpl
    : public ActiveAccountAccessTokenFetcher,
      OAuth2TokenService::Consumer {
 public:
  ActiveAccountAccessTokenFetcherImpl(
      const std::string& active_account_id,
      const std::string& oauth_consumer_name,
      OAuth2TokenService* token_service,
      const OAuth2TokenService::ScopeSet& scopes,
      ActiveAccountAccessTokenCallback callback);
  ~ActiveAccountAccessTokenFetcherImpl() override = default;

 private:
  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Invokes |callback_| with (|access_token|, |error|).
  void HandleTokenRequestCompletion(const OAuth2TokenService::Request* request,
                                    const GoogleServiceAuthError& error,
                                    const std::string& access_token);

  ActiveAccountAccessTokenCallback callback_;
  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(ActiveAccountAccessTokenFetcherImpl);
};

ActiveAccountAccessTokenFetcherImpl::ActiveAccountAccessTokenFetcherImpl(
    const std::string& active_account_id,
    const std::string& oauth_consumer_name,
    OAuth2TokenService* token_service,
    const OAuth2TokenService::ScopeSet& scopes,
    ActiveAccountAccessTokenCallback callback)
    : OAuth2TokenService::Consumer(oauth_consumer_name),
      callback_(std::move(callback)) {
  access_token_request_ =
      token_service->StartRequest(active_account_id, scopes, this);
}

void ActiveAccountAccessTokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  HandleTokenRequestCompletion(request, GoogleServiceAuthError::AuthErrorNone(),
                               access_token);
}

void ActiveAccountAccessTokenFetcherImpl::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  HandleTokenRequestCompletion(request, error, std::string());
}

void ActiveAccountAccessTokenFetcherImpl::HandleTokenRequestCompletion(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2TokenService::Request> request_deleter(
      std::move(access_token_request_));

  std::move(callback_).Run(error, access_token);
}

IdentityProvider::Observer::~Observer() {}

IdentityProvider::~IdentityProvider() {}

std::unique_ptr<ActiveAccountAccessTokenFetcher>
IdentityProvider::FetchAccessToken(const std::string& oauth_consumer_name,
                                   const OAuth2TokenService::ScopeSet& scopes,

                                   ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<ActiveAccountAccessTokenFetcherImpl>(
      GetActiveAccountId(), oauth_consumer_name, GetTokenService(), scopes,
      std::move(callback));
}

void IdentityProvider::InvalidateAccessToken(
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token) {
  GetTokenService()->InvalidateAccessToken(GetActiveAccountId(), scopes,
                                           access_token);
}

void IdentityProvider::AddObserver(Observer* observer) {
  // See the comment on |num_observers_| in the .h file for why this addition
  // must happen here.
  if (num_observers_ == 0) {
    OAuth2TokenService* token_service = GetTokenService();
    if (token_service)
      token_service->AddObserver(this);
  }

  num_observers_++;
  observers_.AddObserver(observer);
}

void IdentityProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  num_observers_--;

  // See the comment on |num_observers_| in the .h file for why this removal
  // must happen here.
  if (num_observers_ == 0) {
    OAuth2TokenService* token_service = GetTokenService();
    if (token_service)
      token_service->RemoveObserver(this);
  }
}

void IdentityProvider::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenUpdated();
}

void IdentityProvider::OnRefreshTokenRevoked(const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenRemoved();
}

IdentityProvider::IdentityProvider() : num_observers_(0) {}

void IdentityProvider::FireOnActiveAccountLogin() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogin();
}

void IdentityProvider::FireOnActiveAccountLogout() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogout();
}

}  // namespace invalidation
