// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

#include <utility>

#include "base/logging.h"

namespace identity {

PrimaryAccountAccessTokenFetcher::PrimaryAccountAccessTokenFetcher(
    const std::string& oauth_consumer_name,
    SigninManagerBase* signin_manager,
    OAuth2TokenService* token_service,
    const OAuth2TokenService::ScopeSet& scopes,
    AccessTokenFetcher::TokenCallback callback,
    Mode mode)
    : oauth_consumer_name_(oauth_consumer_name),
      signin_manager_(signin_manager),
      token_service_(token_service),
      scopes_(scopes),
      callback_(std::move(callback)),
      signin_manager_observer_(this),
      token_service_observer_(this),
      access_token_retried_(false),
      mode_(mode) {
  if (mode_ == Mode::kImmediate || AreCredentialsAvailable()) {
    StartAccessTokenRequest();
    return;
  }

  // Start observing the SigninManager and Token Service. These observers will
  // be removed either when credentials are obtained and an access token request
  // is started or when this object is destroyed.
  signin_manager_observer_.Add(signin_manager_);
  token_service_observer_.Add(token_service_);
}

PrimaryAccountAccessTokenFetcher::~PrimaryAccountAccessTokenFetcher() {}

bool PrimaryAccountAccessTokenFetcher::AreCredentialsAvailable() const {
  DCHECK_EQ(Mode::kWaitUntilAvailable, mode_);

  return (signin_manager_->IsAuthenticated() &&
          token_service_->RefreshTokenIsAvailable(
              signin_manager_->GetAuthenticatedAccountId()));
}

void PrimaryAccountAccessTokenFetcher::StartAccessTokenRequest() {
  DCHECK(mode_ == Mode::kImmediate || AreCredentialsAvailable());

  // By the time of starting an access token request, we should no longer be
  // listening for signin-related events.
  DCHECK(!signin_manager_observer_.IsObserving(signin_manager_));
  DCHECK(!token_service_observer_.IsObserving(token_service_));

  // Note: We might get here even in cases where we know that there's no refresh
  // token. We're requesting an access token anyway, so that the token service
  // will generate an appropriate error code that we can return to the client.
  DCHECK(!access_token_fetcher_);

  access_token_fetcher_ = std::make_unique<AccessTokenFetcher>(
      signin_manager_->GetAuthenticatedAccountId(), oauth_consumer_name_,
      token_service_, scopes_,
      base::BindOnce(
          &PrimaryAccountAccessTokenFetcher::OnAccessTokenFetchComplete,
          base::Unretained(this)));
}

void PrimaryAccountAccessTokenFetcher::GoogleSigninSucceeded(
    const std::string& account_id,
    const std::string& username) {
  ProcessSigninStateChange();
}

void PrimaryAccountAccessTokenFetcher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  ProcessSigninStateChange();
}

void PrimaryAccountAccessTokenFetcher::ProcessSigninStateChange() {
  DCHECK_EQ(Mode::kWaitUntilAvailable, mode_);

  if (!AreCredentialsAvailable())
    return;

  signin_manager_observer_.Remove(signin_manager_);
  token_service_observer_.Remove(token_service_);

  StartAccessTokenRequest();
}

void PrimaryAccountAccessTokenFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    std::string access_token) {
  access_token_fetcher_.reset();

  // There is a special case for Android that RefreshTokenIsAvailable and
  // StartRequest are called to pre-fetch the account image and name before
  // sign-in. In that case, our ongoing access token request gets cancelled.
  // Moreover, OnRefreshTokenAvailable might happen after startup when the
  // credentials are changed/updated.
  // To handle these cases, we retry a canceled request once.
  // However, a request may also get cancelled for legitimate reasons, e.g.
  // because the user signed out. In those cases, there's no point in retrying,
  // so only retry if there (still) is a valid refresh token.
  // NOTE: Maybe we should retry for all transient errors here, so that clients
  // don't have to.
  if (mode_ == Mode::kWaitUntilAvailable && !access_token_retried_ &&
      error.state() == GoogleServiceAuthError::State::REQUEST_CANCELED &&
      AreCredentialsAvailable()) {
    access_token_retried_ = true;
    StartAccessTokenRequest();
    return;
  }

  // Per the contract of this class, it is allowed for consumers to delete this
  // object from within the callback that is run below. Hence, it is not safe to
  // add any code below this call.
  std::move(callback_).Run(error, access_token);
}

}  // namespace identity
