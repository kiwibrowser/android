// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "net/base/backoff_entry.h"

namespace identity {
class IdentityManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// ProfileOAuth2TokenService is a KeyedService that retrieves
// OAuth2 access tokens for a given set of scopes using the OAuth2 login
// refresh tokens.
//
// See |OAuth2TokenService| for usage details.
//
// Note: after StartRequest returns, in-flight requests will continue
// even if the TokenService refresh token that was used to initiate
// the request changes or is cleared.  When the request completes,
// Consumer::OnGetTokenSuccess will be invoked, but the access token
// won't be cached.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use OAuth2TokenServiceRequest.
class ProfileOAuth2TokenService : public OAuth2TokenService,
                                  public OAuth2TokenService::Observer,
                                  public KeyedService {
 public:
  ProfileOAuth2TokenService(
      std::unique_ptr<OAuth2TokenServiceDelegate> delegate);
  ~ProfileOAuth2TokenService() override;

  // Registers per-profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // KeyedService implementation.
  void Shutdown() override;

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  //
  // The primary account is specified with the |primary_account_id| argument.
  // For a regular profile, the primary account id comes from SigninManager.
  // For a supervised user, the id comes from SupervisedUserService.
  void LoadCredentials(const std::string& primary_account_id);

  // Returns true iff all credentials have been loaded from disk.
  bool AreAllCredentialsLoaded();

  // Updates a |refresh_token| for an |account_id|. Credentials are persisted,
  // and available through |LoadCredentials| after service is restarted.
  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token);

  virtual void RevokeCredentials(const std::string& account_id);

  // Returns a pointer to its instance of net::BackoffEntry or nullptr if there
  // is no such instance.
  const net::BackoffEntry* GetDelegateBackoffEntry();

  void set_all_credentials_loaded_for_testing(bool loaded) {
    all_credentials_loaded_ = loaded;
  }

 private:
  friend class identity::IdentityManager;

  // Interface that gives information on internal TokenService operations. Only
  // for use by IdentityManager during the conversion of the codebase to use
  // //services/identity/public/cpp.
  // NOTE: This interface is defined on ProfileOAuth2TokenService rather than
  // on the OAuth2TokenService base class for multiple reasons:
  // (1) The base class already has a DiagnosticsObserver interface, from
  // which this interface differs because there can be only one instance.
  // (2) PO2TS itself observes O2TS and for correctness must receive observer
  // callbacks before any other O2TS observer. Hence, these DiagnosticsClient
  // callouts must go *inside* PO2TS's implementations of the O2TS observer
  // methods.
  class DiagnosticsClient {
   public:
    // Sent just before OnRefreshTokenAvailable() is fired on observers.
    // |is_valid| indicates whether the token is valid.
    virtual void WillFireOnRefreshTokenAvailable(const std::string& account_id,
                                                 bool is_valid) = 0;
    // Sent just before OnRefreshTokenRevoked() is fired on observers.
    virtual void WillFireOnRefreshTokenRevoked(
        const std::string& account_id) = 0;
  };

  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

  void set_diagnostics_client(DiagnosticsClient* diagnostics_client) {
    DCHECK(!diagnostics_client_ || !diagnostics_client);
    diagnostics_client_ = diagnostics_client;
  }

  // Whether all credentials have been loaded.
  bool all_credentials_loaded_;

  // The DiagnosticsClient object associated with this object. May be null.
  DiagnosticsClient* diagnostics_client_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_H_
