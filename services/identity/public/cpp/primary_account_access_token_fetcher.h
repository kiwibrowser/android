// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "services/identity/public/cpp/access_token_fetcher.h"

namespace identity {

// Helper class to ease the task of obtaining an OAuth2 access token for the
// authenticated account. This handles various special cases, e.g. when the
// refresh token isn't loaded yet (during startup), or when there is some
// transient error.
// May only be used on the UI thread.
class PrimaryAccountAccessTokenFetcher : public SigninManagerBase::Observer,
                                         public OAuth2TokenService::Observer {
 public:
  // Specifies how this instance should behave:
  // |kImmediate|: Makes one-shot immediate request.
  // |kWaitUntilAvailable|: Waits for the primary account to be available
  // before making the request.
  // Note that using |kWaitUntilAvailable| can result in waiting forever
  // if the user is not signed in and doesn't sign in.
  enum class Mode { kImmediate, kWaitUntilAvailable };

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for the given |scopes|. The |callback| is called once
  // the request completes (successful or not). If the
  // PrimaryAccountAccessTokenFetcher is destroyed before the process completes,
  // the callback is not called.
  PrimaryAccountAccessTokenFetcher(const std::string& oauth_consumer_name,
                                   SigninManagerBase* signin_manager,
                                   OAuth2TokenService* token_service,
                                   const OAuth2TokenService::ScopeSet& scopes,
                                   AccessTokenFetcher::TokenCallback callback,
                                   Mode mode);

  ~PrimaryAccountAccessTokenFetcher() override;

 private:
  // Returns true iff there is a primary account with a refresh token. Should
  // only be called in mode |kWaitUntilAvailable|.
  bool AreCredentialsAvailable() const;

  void StartAccessTokenRequest();

  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override;

  // OAuth2TokenService::Observer implementation.
  void OnRefreshTokenAvailable(const std::string& account_id) override;

  // Checks whether credentials are now available and starts an access token
  // request if so. Should only be called in mode |kWaitUntilAvailable|.
  void ProcessSigninStateChange();

  // Invoked by |fetcher_| when an access token request completes.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  std::string access_token);

  std::string oauth_consumer_name_;
  SigninManagerBase* signin_manager_;
  OAuth2TokenService* token_service_;
  OAuth2TokenService::ScopeSet scopes_;

  // Per the contract of this class, it is allowed for clients to delete this
  // object as part of the invocation of |callback_|. Hence, this object must
  // assume that it is dead after invoking |callback_| and must not run any more
  // code.
  AccessTokenFetcher::TokenCallback callback_;

  ScopedObserver<SigninManagerBase, PrimaryAccountAccessTokenFetcher>
      signin_manager_observer_;
  ScopedObserver<OAuth2TokenService, PrimaryAccountAccessTokenFetcher>
      token_service_observer_;

  // Internal fetcher that does the actual access token request.
  std::unique_ptr<AccessTokenFetcher> access_token_fetcher_;

  // When a token request gets canceled, we want to retry once.
  bool access_token_retried_;

  Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryAccountAccessTokenFetcher);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_PRIMARY_ACCOUNT_ACCESS_TOKEN_FETCHER_H_
