// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/signin/core/browser/account_info.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/connection_status.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}

namespace syncer {
struct SyncCredentials;
class SyncPrefs;
}  // namespace syncer

namespace browser_sync {

// SyncAuthManager tracks the primary (i.e. blessed-for-sync) account and its
// authentication state.
class SyncAuthManager : public identity::IdentityManager::Observer {
 public:
  // Called when the existence of an authenticated account changes. Call
  // GetAuthenticatedAccountInfo to get the new state.
  using AccountStateChangedCallback = base::RepeatingClosure;
  // Called when the credential state changes, i.e. an access token was
  // added/changed/removed. Call GetCredentials to get the new state.
  using CredentialsChangedCallback = base::RepeatingClosure;

  // |sync_prefs| must not be null and must outlive this.
  // |identity_manager| may be null (this is the case if local Sync is enabled),
  // but if non-null, must outlive this object.
  SyncAuthManager(syncer::SyncPrefs* sync_prefs,
                  identity::IdentityManager* identity_manager,
                  const AccountStateChangedCallback& account_state_changed,
                  const CredentialsChangedCallback& credentials_changed);
  ~SyncAuthManager() override;

  // Tells the tracker to start listening for changes to the account/sign-in
  // status. This gets called during SyncService initialization, except in the
  // case of local Sync.
  void RegisterForAuthNotifications();

  // Returns the AccountInfo for the primary (i.e. blessed-for-sync) account, or
  // an empty AccountInfo if there isn't one.
  AccountInfo GetAuthenticatedAccountInfo() const;

  const GoogleServiceAuthError& GetLastAuthError() const {
    return last_auth_error_;
  }

  // Returns the credentials to be passed to the SyncEngine.
  syncer::SyncCredentials GetCredentials() const;

  const std::string& access_token() const { return access_token_; }

  // Returns the state of the access token and token request, for display in
  // internals UI.
  const syncer::SyncTokenStatus& GetSyncTokenStatus() const;

  // Called by ProfileSyncService when the status of the connection to the Sync
  // server changed. Updates auth error state accordingly.
  void ConnectionStatusChanged(syncer::ConnectionStatus status);

  // Clears all auth-related state (error, cached access token etc). Called
  // when Sync is turned off.
  void Clear();

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const AccountInfo& previous_primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;
  void OnRefreshTokenRemovedForAccount(
      const AccountInfo& account_info) override;

  // Test-only methods for inspecting/modifying internal state.
  bool IsRetryingAccessTokenFetchForTest() const;
  void ResetRequestAccessTokenBackoffForTest();

 private:
  void UpdateAuthErrorState(const GoogleServiceAuthError& error);
  void ClearAuthError();

  void ClearAccessTokenAndRequest();

  void RequestAccessToken();

  void AccessTokenFetched(GoogleServiceAuthError error,
                          std::string access_token);

  syncer::SyncPrefs* const sync_prefs_;
  identity::IdentityManager* const identity_manager_;

  const AccountStateChangedCallback account_state_changed_callback_;
  const CredentialsChangedCallback credentials_changed_callback_;

  bool registered_for_auth_notifications_;

  // This is a cache of the last authentication response we received either
  // from the sync server or from Chrome's identity/token management system.
  GoogleServiceAuthError last_auth_error_;

  // The current access token. This is mutually exclusive with
  // |ongoing_access_token_fetch_| and |request_access_token_retry_timer_|:
  // We either have an access token OR a pending request OR a pending retry.
  std::string access_token_;

  // Pending request for an access token. Non-null iff there is a request
  // ongoing.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      ongoing_access_token_fetch_;

  // If RequestAccessToken fails with transient error then retry requesting
  // access token with exponential backoff.
  base::OneShotTimer request_access_token_retry_timer_;
  net::BackoffEntry request_access_token_backoff_;

  // Info about the state of our access token, for display in the internals UI.
  syncer::SyncTokenStatus token_status_;

  base::WeakPtrFactory<SyncAuthManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SyncAuthManager);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_AUTH_MANAGER_H_
