// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace invalidation {

// An opaque object that clients can use to control the lifetime of access
// token requests.
class ActiveAccountAccessTokenFetcher {
 public:
  ActiveAccountAccessTokenFetcher() = default;
  virtual ~ActiveAccountAccessTokenFetcher() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveAccountAccessTokenFetcher);
};

using ActiveAccountAccessTokenCallback =
    base::OnceCallback<void(GoogleServiceAuthError error,
                            std::string access_token)>;

// Helper class that provides access to information about the "active GAIA
// account" with which invalidation should interact. The definition of the
// "active Gaia account is context-dependent": the purpose of this abstraction
// layer is to allow invalidation to interact with either device identity or
// user identity via a uniform interface.
class IdentityProvider : public OAuth2TokenService::Observer {
 public:
  class Observer {
   public:
    // Called when a GAIA account logs in and becomes the active account. All
    // account information is available when this method is called and all
    // |IdentityProvider| methods will return valid data.
    virtual void OnActiveAccountLogin() {}

    // Called when the active GAIA account logs out. The account information may
    // have been cleared already when this method is called. The
    // |IdentityProvider| methods may return inconsistent or outdated
    // information if called from within OnLogout().
    virtual void OnActiveAccountLogout() {}

    // Called when the active GAIA account's refresh token is updated.
    virtual void OnActiveAccountRefreshTokenUpdated() {}

    // Called when the active GAIA account's refresh token is removed.
    virtual void OnActiveAccountRefreshTokenRemoved() {}

   protected:
    virtual ~Observer();
  };

  ~IdentityProvider() override;

  // Gets the active account's account ID.
  virtual std::string GetActiveAccountId() = 0;

  // Returns true iff (1) there is an active account and (2) that account has
  // a refresh token.
  virtual bool IsActiveAccountAvailable() = 0;

  // Starts an access token request for |oauth_consumer_name| and |scopes|. When
  // the request completes, |callback| will be invoked with the access token
  // or error. To cancel the request, destroy the returned TokenFetcher.
  std::unique_ptr<ActiveAccountAccessTokenFetcher> FetchAccessToken(
      const std::string& oauth_consumer_name,
      const OAuth2TokenService::ScopeSet& scopes,

      ActiveAccountAccessTokenCallback callback);

  // Marks an OAuth2 |access_token| issued for the active account and |scopes|
  // as invalid.
  void InvalidateAccessToken(const OAuth2TokenService::ScopeSet& scopes,
                             const std::string& access_token);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;

 protected:
  IdentityProvider();

  // DEPRECATED: Do not add further usage of this API, as it is in the process
  // of being removed. See https://crbug.com/809452.
  // Gets the token service vending OAuth tokens for all logged-in accounts.
  virtual OAuth2TokenService* GetTokenService() = 0;

  // Fires an OnActiveAccountLogin notification.
  void FireOnActiveAccountLogin();

  // Fires an OnActiveAccountLogout notification.
  void FireOnActiveAccountLogout();

 private:
  base::ObserverList<Observer, true> observers_;

  // Tracks the number of observers in order to allow for adding/removing this
  // object as an observer of GetTokenService() when the observer count flips
  // from/to 0. This is necessary because GetTokenService() is a pure virtual
  // method and cannot be called in the destructor of this object.
  // TODO(809452): Push observing of the token service into the subclasses of
  // this class.
  int num_observers_;

  DISALLOW_COPY_AND_ASSIGN(IdentityProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_
