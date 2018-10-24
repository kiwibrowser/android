// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/access_token_fetcher.h"

#include <utility>

#include "base/logging.h"

namespace identity {

AccessTokenFetcher::AccessTokenFetcher(
    const std::string& account_id,
    const std::string& oauth_consumer_name,
    OAuth2TokenService* token_service,
    const OAuth2TokenService::ScopeSet& scopes,
    TokenCallback callback)
    : OAuth2TokenService::Consumer(oauth_consumer_name),
      account_id_(account_id),
      token_service_(token_service),
      scopes_(scopes),
      callback_(std::move(callback)) {
  // TODO(843510): Consider making the request to ProfileOAuth2TokenService
  // asynchronously once there are no direct clients of PO2TS (i.e., PO2TS is
  // used only by this class and IdentityManager).
  access_token_request_ =
      token_service_->StartRequest(account_id_, scopes_, this);
}

AccessTokenFetcher::~AccessTokenFetcher() {}

void AccessTokenFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2TokenService::Request> request_deleter(
      std::move(access_token_request_));

  RunCallbackAndMaybeDie(GoogleServiceAuthError::AuthErrorNone(), access_token);

  // Potentially dead after the above invocation; nothing to do except return.
}

void AccessTokenFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2TokenService::Request> request_deleter(
      std::move(access_token_request_));

  RunCallbackAndMaybeDie(error, std::string());

  // Potentially dead after the above invocation; nothing to do except return.
}

void AccessTokenFetcher::RunCallbackAndMaybeDie(
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  // Per the contract of this class, it is allowed for consumers to delete this
  // object from within the callback that is run below. Hence, it is not safe to
  // add any code below this call.
  std::move(callback_).Run(error, access_token);
}

}  // namespace identity
