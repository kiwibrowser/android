// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_auth_manager.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/sync/base/stop_source.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/engine/sync_credentials.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

namespace browser_sync {

namespace {

constexpr char kSyncOAuthConsumerName[] = "sync";

constexpr net::BackoffEntry::Policy kRequestAccessTokenBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    2000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.2,  // 20%

    // Maximum amount of time we are willing to delay our request in ms.
    // TODO(crbug.com/246686): We should retry RequestAccessToken on connection
    // state change after backoff.
    1000 * 3600 * 4,  // 4 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Don't use initial delay unless the last request was an error.
    false,
};

}  // namespace

SyncAuthManager::SyncAuthManager(
    syncer::SyncPrefs* sync_prefs,
    identity::IdentityManager* identity_manager,
    const AccountStateChangedCallback& account_state_changed,
    const CredentialsChangedCallback& credentials_changed)
    : sync_prefs_(sync_prefs),
      identity_manager_(identity_manager),
      account_state_changed_callback_(account_state_changed),
      credentials_changed_callback_(credentials_changed),
      registered_for_auth_notifications_(false),
      request_access_token_backoff_(&kRequestAccessTokenBackoffPolicy),
      weak_ptr_factory_(this) {
  DCHECK(sync_prefs_);
  // |identity_manager_|can be null if local Sync is enabled.
}

SyncAuthManager::~SyncAuthManager() {
  if (registered_for_auth_notifications_) {
    identity_manager_->RemoveObserver(this);
  }
}

void SyncAuthManager::RegisterForAuthNotifications() {
  DCHECK(!registered_for_auth_notifications_);
  identity_manager_->AddObserver(this);
  registered_for_auth_notifications_ = true;
}

AccountInfo SyncAuthManager::GetAuthenticatedAccountInfo() const {
  return identity_manager_ ? identity_manager_->GetPrimaryAccountInfo()
                           : AccountInfo();
}

const syncer::SyncTokenStatus& SyncAuthManager::GetSyncTokenStatus() const {
  return token_status_;
}

syncer::SyncCredentials SyncAuthManager::GetCredentials() const {
  syncer::SyncCredentials credentials;

  const AccountInfo account_info = GetAuthenticatedAccountInfo();
  credentials.account_id = account_info.account_id;
  credentials.email = account_info.email;
  credentials.sync_token = access_token_;

  credentials.scope_set.insert(GaiaConstants::kChromeSyncOAuth2Scope);

  return credentials;
}

void SyncAuthManager::ConnectionStatusChanged(syncer::ConnectionStatus status) {
  token_status_.connection_status_update_time = base::Time::Now();
  token_status_.connection_status = status;

  switch (status) {
    case syncer::CONNECTION_AUTH_ERROR:
      // Sync server returned error indicating that access token is invalid. It
      // could be either expired or access is revoked. Let's request another
      // access token and if access is revoked then request for token will fail
      // with corresponding error. If access token is repeatedly reported
      // invalid, there may be some issues with server, e.g. authentication
      // state is inconsistent on sync and token server. In that case, we
      // backoff token requests exponentially to avoid hammering token server
      // too much and to avoid getting same token due to token server's caching
      // policy. |request_access_token_retry_timer_| is used to backoff request
      // triggered by both auth error and failure talking to GAIA server.
      // Therefore, we're likely to reach the backoff ceiling more quickly than
      // you would expect from looking at the BackoffPolicy if both types of
      // errors happen. We shouldn't receive two errors back-to-back without
      // attempting a token/sync request in between, thus crank up request delay
      // unnecessary. This is because we won't make a sync request if we hit an
      // error until GAIA succeeds at sending a new token, and we won't request
      // a new token unless sync reports a token failure. But to be safe, don't
      // schedule request if this happens.
      if (request_access_token_retry_timer_.IsRunning()) {
        // The timer to perform a request later is already running; nothing
        // further needs to be done at this point.
      } else if (request_access_token_backoff_.failure_count() == 0) {
        // First time request without delay. Currently invalid token is used
        // to initialize sync engine and we'll always end up here. We don't
        // want to delay initialization.
        request_access_token_backoff_.InformOfRequest(false);
        RequestAccessToken();
      } else {
        request_access_token_backoff_.InformOfRequest(false);
        base::TimeDelta delay =
            request_access_token_backoff_.GetTimeUntilRelease();
        token_status_.next_token_request_time = base::Time::Now() + delay;
        request_access_token_retry_timer_.Start(
            FROM_HERE, delay,
            base::BindRepeating(&SyncAuthManager::RequestAccessToken,
                                weak_ptr_factory_.GetWeakPtr()));
      }
      break;
    case syncer::CONNECTION_OK:
      // Reset backoff time after successful connection.
      // Request shouldn't be scheduled at this time. But if it is, it's
      // possible that sync flips between OK and auth error states rapidly,
      // thus hammers token server. To be safe, only reset backoff delay when
      // no scheduled request.
      if (!request_access_token_retry_timer_.IsRunning()) {
        request_access_token_backoff_.Reset();
      }
      ClearAuthError();
      break;
    case syncer::CONNECTION_SERVER_ERROR:
      UpdateAuthErrorState(
          GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
      break;
    case syncer::CONNECTION_NOT_ATTEMPTED:
      // The connection status should never change to "not attempted".
      NOTREACHED();
      break;
  }
}

void SyncAuthManager::UpdateAuthErrorState(
    const GoogleServiceAuthError& error) {
  last_auth_error_ = error;
}

void SyncAuthManager::ClearAuthError() {
  UpdateAuthErrorState(GoogleServiceAuthError::AuthErrorNone());
}

void SyncAuthManager::ClearAccessTokenAndRequest() {
  access_token_.clear();
  request_access_token_retry_timer_.Stop();
  token_status_.next_token_request_time = base::Time();
  ongoing_access_token_fetch_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void SyncAuthManager::Clear() {
  ClearAuthError();
  ClearAccessTokenAndRequest();
}

void SyncAuthManager::OnPrimaryAccountSet(
    const AccountInfo& primary_account_info) {
  account_state_changed_callback_.Run();
}

void SyncAuthManager::OnPrimaryAccountCleared(
    const AccountInfo& previous_primary_account_info) {
  UMA_HISTOGRAM_ENUMERATION("Sync.StopSource", syncer::SIGN_OUT,
                            syncer::STOP_SOURCE_LIMIT);
  account_state_changed_callback_.Run();
}

void SyncAuthManager::OnRefreshTokenUpdatedForAccount(
    const AccountInfo& account_info,
    bool is_valid) {
  if (account_info.account_id != GetAuthenticatedAccountInfo().account_id) {
    return;
  }

  if (!is_valid) {
    // When the refresh token is replaced by an invalid token, Sync must be
    // stopped immediately, even if the current access token is still valid.
    // This happens e.g. when the user signs out of the web with Dice enabled.
    ClearAccessTokenAndRequest();

    // Set the last auth error to the one that is specified in
    // google_service_auth_error.h to correspond to this case (token was
    // invalidated client-side).
    // TODO(blundell): Long-term, it would be nicer if Sync didn't have to
    // cache signin-level authentication errors.
    GoogleServiceAuthError invalid_token_error =
        GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
            GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT);
    UpdateAuthErrorState(invalid_token_error);

    credentials_changed_callback_.Run();
    return;
  }

  // If we already have an access token or previously failed to retrieve one
  // (and hence the retry timer is running), then request a fresh access token
  // now. This will also drop the current access token.
  if (!access_token_.empty() || request_access_token_retry_timer_.IsRunning()) {
    DCHECK(!ongoing_access_token_fetch_);
    RequestAccessToken();
  } else if (last_auth_error_ != GoogleServiceAuthError::AuthErrorNone()) {
    // If we were in an auth error state, then now's also a good time to try
    // again. In this case it's possible that there is already a pending
    // request, in which case RequestAccessToken will simply do nothing.
    RequestAccessToken();
  }
}

void SyncAuthManager::OnRefreshTokenRemovedForAccount(
    const AccountInfo& account_info) {
  if (account_info.account_id != GetAuthenticatedAccountInfo().account_id) {
    return;
  }

  UpdateAuthErrorState(
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  ClearAccessTokenAndRequest();

  credentials_changed_callback_.Run();
}

bool SyncAuthManager::IsRetryingAccessTokenFetchForTest() const {
  return request_access_token_retry_timer_.IsRunning();
}

void SyncAuthManager::ResetRequestAccessTokenBackoffForTest() {
  request_access_token_backoff_.Reset();
}

void SyncAuthManager::RequestAccessToken() {
  // Only one active request at a time.
  if (ongoing_access_token_fetch_) {
    return;
  }
  request_access_token_retry_timer_.Stop();
  token_status_.next_token_request_time = base::Time();

  OAuth2TokenService::ScopeSet oauth2_scopes;
  oauth2_scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);

  // Invalidate previous token, otherwise token service will return the same
  // token again.
  if (!access_token_.empty()) {
    identity_manager_->RemoveAccessTokenFromCache(GetAuthenticatedAccountInfo(),
                                                  oauth2_scopes, access_token_);

    access_token_.clear();
    credentials_changed_callback_.Run();
  }


  token_status_.token_request_time = base::Time::Now();
  token_status_.token_receive_time = base::Time();
  token_status_.next_token_request_time = base::Time();
  ongoing_access_token_fetch_ =
      identity_manager_->CreateAccessTokenFetcherForPrimaryAccount(
          kSyncOAuthConsumerName, oauth2_scopes,
          base::BindOnce(&SyncAuthManager::AccessTokenFetched,
                         base::Unretained(this)),
          identity::PrimaryAccountAccessTokenFetcher::Mode::
              kWaitUntilAvailable);
}

void SyncAuthManager::AccessTokenFetched(GoogleServiceAuthError error,
                                         std::string access_token) {
  DCHECK(ongoing_access_token_fetch_);
  ongoing_access_token_fetch_.reset();

  access_token_ = access_token;
  token_status_.last_get_token_error = error;

  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
      token_status_.token_receive_time = base::Time::Now();
      sync_prefs_->SetSyncAuthError(false);
      ClearAuthError();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      // Transient error. Retry after some time.
      request_access_token_backoff_.InformOfRequest(false);
      token_status_.next_token_request_time =
          base::Time::Now() +
          request_access_token_backoff_.GetTimeUntilRelease();
      request_access_token_retry_timer_.Start(
          FROM_HERE, request_access_token_backoff_.GetTimeUntilRelease(),
          base::BindRepeating(&SyncAuthManager::RequestAccessToken,
                              weak_ptr_factory_.GetWeakPtr()));
      break;
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
      sync_prefs_->SetSyncAuthError(true);
      UpdateAuthErrorState(error);
      break;
    default:
      LOG(ERROR) << "Unexpected persistent error: " << error.ToString();
      UpdateAuthErrorState(error);
  }

  credentials_changed_callback_.Run();
}

}  // namespace browser_sync
