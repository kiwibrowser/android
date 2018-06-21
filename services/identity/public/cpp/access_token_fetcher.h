// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_PUBLIC_CPP_ACCESS_TOKEN_FETCHER_H_
#define SERVICES_IDENTITY_PUBLIC_CPP_ACCESS_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace identity {

// Helper class to ease the task of obtaining an OAuth2 access token for a
// given account.
// May only be used on the UI thread.
class AccessTokenFetcher : public OAuth2TokenService::Observer,
                           public OAuth2TokenService::Consumer {
 public:
  // Callback for when a request completes (successful or not). On successful
  // requests, |error| is NONE and |access_token| contains the obtained OAuth2
  // access token. On failed requests, |error| contains the actual error and
  // |access_token| is empty.
  // NOTE: At the time that this method is invoked, it is safe for the client to
  // destroy the AccessTokenFetcher instance that is invoking this callback.
  using TokenCallback = base::OnceCallback<void(GoogleServiceAuthError error,
                                                std::string access_token)>;

  // Instantiates a fetcher and immediately starts the process of obtaining an
  // OAuth2 access token for |account_id| and |scopes|. The |callback| is called
  // once the request completes (successful or not). If the AccessTokenFetcher
  // is destroyed before the process completes, the callback is not called.
  AccessTokenFetcher(const std::string& account_id,
                     const std::string& oauth_consumer_name,
                     OAuth2TokenService* token_service,
                     const OAuth2TokenService::ScopeSet& scopes,
                     TokenCallback callback);

  ~AccessTokenFetcher() override;

 private:
  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Invokes |callback_| with (|error|, |access_token|). Per the contract of
  // this class, it is allowed for clients to delete this object as part of the
  // invocation of |callback_|. Hence, this object must assume that it is dead
  // after invoking this method and must not run any more code.
  void RunCallbackAndMaybeDie(const GoogleServiceAuthError& error,
                              const std::string& access_token);

  std::string account_id_;
  OAuth2TokenService* token_service_;
  OAuth2TokenService::ScopeSet scopes_;

  // NOTE: This callback should only be invoked from |RunCallbackAndMaybeDie|,
  // as invoking it has the potential to destroy this object per this class's
  // contract.
  TokenCallback callback_;

  std::unique_ptr<OAuth2TokenService::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(AccessTokenFetcher);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_PUBLIC_CPP_ACCESS_TOKEN_FETCHER_H_
