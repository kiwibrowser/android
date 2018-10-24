// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_auth_manager.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/engine/connection_status.h"
#include "components/sync/engine/sync_credentials.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/base/net_errors.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

class SyncAuthManagerTest : public testing::Test {
 protected:
  using AccountStateChangedCallback =
      SyncAuthManager::AccountStateChangedCallback;
  using CredentialsChangedCallback =
      SyncAuthManager::CredentialsChangedCallback;

  SyncAuthManagerTest() {
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    sync_prefs_ = std::make_unique<syncer::SyncPrefs>(&pref_service_);
  }

  ~SyncAuthManagerTest() override {}

  std::unique_ptr<SyncAuthManager> CreateAuthManager() {
    return CreateAuthManager(base::DoNothing(), base::DoNothing());
  }

  std::unique_ptr<SyncAuthManager> CreateAuthManager(
      const AccountStateChangedCallback& account_state_changed,
      const CredentialsChangedCallback& credentials_changed) {
    return std::make_unique<SyncAuthManager>(
        sync_prefs_.get(), identity_env_.identity_manager(),
        account_state_changed, credentials_changed);
  }

  std::unique_ptr<SyncAuthManager> CreateAuthManagerForLocalSync() {
    return std::make_unique<SyncAuthManager>(
        sync_prefs_.get(), nullptr, base::DoNothing(), base::DoNothing());
  }

  identity::IdentityTestEnvironment* identity_env() { return &identity_env_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  identity::IdentityTestEnvironment identity_env_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<syncer::SyncPrefs> sync_prefs_;
};

TEST_F(SyncAuthManagerTest, ProvidesNothingInLocalSyncMode) {
  auto auth_manager = CreateAuthManagerForLocalSync();
  EXPECT_TRUE(auth_manager->GetAuthenticatedAccountInfo().IsEmpty());
  syncer::SyncCredentials credentials = auth_manager->GetCredentials();
  EXPECT_TRUE(credentials.account_id.empty());
  EXPECT_TRUE(credentials.email.empty());
  EXPECT_TRUE(credentials.sync_token.empty());
  EXPECT_TRUE(auth_manager->access_token().empty());
  // Note: Calling RegisterForAuthNotifications is illegal in local Sync mode,
  // so we don't test that.
  // Calling Clear() does nothing, but shouldn't crash.
  auth_manager->Clear();
}

// ChromeOS doesn't support sign-in/sign-out.
#if !defined(OS_CHROMEOS)
TEST_F(SyncAuthManagerTest, IgnoresEventsIfNotRegistered) {
  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  auto auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());

  // Fire some auth events. We haven't called RegisterForAuthNotifications, so
  // none of this should result in any callback calls.
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  identity_env()->SetRefreshTokenForPrimaryAccount();
  identity_env()->ClearPrimaryAccount();
  ASSERT_TRUE(auth_manager->GetAuthenticatedAccountInfo().account_id.empty());
}

TEST_F(SyncAuthManagerTest, ForwardsPrimaryAccountEvents) {
  // Start out already signed in before the SyncAuthManager is created.
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;

  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  auto auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());

  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);

  auth_manager->RegisterForAuthNotifications();

  // Sign out of the account.
  EXPECT_CALL(account_state_changed, Run());
  // Note: The ordering of removing the refresh token and the actual sign-out is
  // undefined, see comment on IdentityManager::Observer. So we might or might
  // not get a |credentials_changed| call here.
  EXPECT_CALL(credentials_changed, Run()).Times(testing::AtMost(1));
  identity_env()->ClearPrimaryAccount();
  EXPECT_TRUE(auth_manager->GetAuthenticatedAccountInfo().account_id.empty());

  // Sign in to a different account.
  EXPECT_CALL(account_state_changed, Run());
  std::string second_account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  EXPECT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id,
            second_account_id);
}
#endif  // !OS_CHROMEOS

TEST_F(SyncAuthManagerTest, ForwardsCredentialsEvents) {
  // Start out already signed in before the SyncAuthManager is created.
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;

  base::MockCallback<AccountStateChangedCallback> account_state_changed;
  base::MockCallback<CredentialsChangedCallback> credentials_changed;
  EXPECT_CALL(account_state_changed, Run()).Times(0);
  EXPECT_CALL(credentials_changed, Run()).Times(0);
  auto auth_manager =
      CreateAuthManager(account_state_changed.Get(), credentials_changed.Get());

  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);

  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  // Once an access token is available, the callback should get run.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // Now the refresh token gets updated. The access token will get dropped, so
  // this should cause another notification.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->SetRefreshTokenForPrimaryAccount();
  ASSERT_TRUE(auth_manager->GetCredentials().sync_token.empty());

  // Once a new token is available, there's another notification.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token_2");

  // Revoking the refresh token should also cause the access token to get
  // dropped.
  EXPECT_CALL(credentials_changed, Run());
  identity_env()->RemoveRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(auth_manager->GetCredentials().sync_token.empty());
}

TEST_F(SyncAuthManagerTest, RequestsAccessTokenOnSyncStartup) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));

  EXPECT_EQ(auth_manager->GetCredentials().sync_token, "access_token");
}

TEST_F(SyncAuthManagerTest,
       RetriesAccessTokenFetchWithBackoffOnTransientFailure) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(net::ERR_TIMED_OUT));

  // The access token fetch should get retried (with backoff, hence no actual
  // request yet), without exposing an auth error.
  EXPECT_TRUE(auth_manager->IsRetryingAccessTokenFetchForTest());
  EXPECT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
}

TEST_F(SyncAuthManagerTest, AbortsAccessTokenFetchOnPersistentFailure) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  GoogleServiceAuthError auth_error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      auth_error);

  // Auth error should get exposed; no retry.
  EXPECT_FALSE(auth_manager->IsRetryingAccessTokenFetchForTest());
  EXPECT_EQ(auth_manager->GetLastAuthError(), auth_error);
}

TEST_F(SyncAuthManagerTest, FetchesNewAccessTokenWithBackoffOnServerError) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // But now the server is still returning AUTH_ERROR - maybe something's wrong
  // with the token.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  // The access token fetch should get retried (with backoff, hence no actual
  // request yet), without exposing an auth error.
  EXPECT_TRUE(auth_manager->IsRetryingAccessTokenFetchForTest());
  EXPECT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
}

TEST_F(SyncAuthManagerTest, ExposesServerError) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // Now a server error happens.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_SERVER_ERROR);

  // The error should be reported.
  EXPECT_NE(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());
  // But the access token should still be there - this might just be some
  // non-auth-related problem with the server.
  EXPECT_EQ(auth_manager->GetCredentials().sync_token, "access_token");
}

TEST_F(SyncAuthManagerTest, RequestsNewAccessTokenOnExpiry) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But then the token expires, resulting in an auth error from the server.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);

  // Should immediately drop the access token and fetch a new one (no backoff).
  EXPECT_TRUE(auth_manager->GetCredentials().sync_token.empty());

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::TimeDelta::FromHours(1));
  EXPECT_EQ(auth_manager->GetCredentials().sync_token, "access_token_2");
}

TEST_F(SyncAuthManagerTest, RequestsNewAccessTokenOnRefreshTokenUpdate) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But then the refresh token changes.
  identity_env()->SetRefreshTokenForPrimaryAccount();

  // Should immediately drop the access token and fetch a new one (no backoff).
  EXPECT_TRUE(auth_manager->GetCredentials().sync_token.empty());

  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token_2", base::Time::Now() + base::TimeDelta::FromHours(1));
  EXPECT_EQ(auth_manager->GetCredentials().sync_token, "access_token_2");
}

TEST_F(SyncAuthManagerTest, DoesNotRequestAccessTokenAutonomously) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // Do *not* call ConnectionStatusChanged here (which is what usually kicks off
  // the token fetch).

  // Now the refresh token gets updated. If we already had an access token
  // before, then this should trigger a new fetch. But since that initial fetch
  // never happened (e.g. because Sync is turned off), this should do nothing.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  identity_env()->SetRefreshTokenForPrimaryAccount();

  // Make sure no access token request was sent. Since the request goes through
  // posted tasks, we have to spin the message loop.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(auth_manager->GetCredentials().sync_token.empty());
}

TEST_F(SyncAuthManagerTest, ClearsCredentialsOnRefreshTokenRemoval) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But then the refresh token gets revoked. No new access token should get
  // requested due to this.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  identity_env()->RemoveRefreshTokenForPrimaryAccount();

  // Should immediately drop the access token and expose an auth error.
  EXPECT_TRUE(auth_manager->GetCredentials().sync_token.empty());
  EXPECT_NE(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // No new access token should have been requested. Since the request goes
  // through posted tasks, we have to spin the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncAuthManagerTest, ClearsCredentialsOnInvalidRefreshToken) {
  std::string account_id =
      identity_env()->MakePrimaryAccountAvailable("test@email.com").account_id;
  auto auth_manager = CreateAuthManager();
  ASSERT_EQ(auth_manager->GetAuthenticatedAccountInfo().account_id, account_id);
  auth_manager->RegisterForAuthNotifications();

  // During Sync startup, the SyncEngine attempts to connect to the server
  // without an access token, resulting in a call to ConnectionStatusChanged
  // with CONNECTION_AUTH_ERROR. This is what kicks off the initial access token
  // fetch.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_AUTH_ERROR);
  identity_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::TimeDelta::FromHours(1));
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");

  // Now everything is okay for a while.
  auth_manager->ConnectionStatusChanged(syncer::CONNECTION_OK);
  ASSERT_EQ(auth_manager->GetCredentials().sync_token, "access_token");
  ASSERT_EQ(auth_manager->GetLastAuthError(),
            GoogleServiceAuthError::AuthErrorNone());

  // But now an invalid refresh token gets set. No new access token should get
  // requested due to this.
  base::MockCallback<base::OnceClosure> access_token_requested;
  EXPECT_CALL(access_token_requested, Run()).Times(0);
  identity_env()->SetCallbackForNextAccessTokenRequest(
      access_token_requested.Get());
  identity_env()->SetInvalidRefreshTokenForPrimaryAccount();

  // Should immediately drop the access token and expose a special auth error.
  EXPECT_TRUE(auth_manager->GetCredentials().sync_token.empty());
  GoogleServiceAuthError invalid_token_error =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  EXPECT_EQ(auth_manager->GetLastAuthError(), invalid_token_error);

  // No new access token should have been requested. Since the request goes
  // through posted tasks, we have to spin the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace browser_sync
