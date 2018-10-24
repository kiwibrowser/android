// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/access_token_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using sync_preferences::TestingPrefServiceSyncable;
using testing::StrictMock;

namespace identity {

namespace {

const char kTestGaiaId[] = "dummyId";
const char kTestGaiaId2[] = "dummyId2";
const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";

}  // namespace

class AccessTokenFetcherTest : public testing::Test,
                               public OAuth2TokenService::DiagnosticsObserver {
 public:
  using TestTokenCallback =
      StrictMock<MockCallback<AccessTokenFetcher::TokenCallback>>;

  AccessTokenFetcherTest() : signin_client_(&pref_service_) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());

    account_tracker_ = std::make_unique<AccountTrackerService>();
    account_tracker_->Initialize(&signin_client_);

    token_service_.AddDiagnosticsObserver(this);
  }

  ~AccessTokenFetcherTest() override {
    token_service_.RemoveDiagnosticsObserver(this);
  }

  std::string AddAccount(const std::string& gaia_id, const std::string& email) {
    account_tracker()->SeedAccountInfo(gaia_id, email);
    return account_tracker()->FindAccountInfoByGaiaId(gaia_id).account_id;
  }

  std::unique_ptr<AccessTokenFetcher> CreateFetcher(
      const std::string& account_id,
      AccessTokenFetcher::TokenCallback callback) {
    std::set<std::string> scopes{"scope"};
    return std::make_unique<AccessTokenFetcher>(account_id, "test_consumer",
                                                &token_service_, scopes,
                                                std::move(callback));
  }

  AccountTrackerService* account_tracker() { return account_tracker_.get(); }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

  void set_on_access_token_request_callback(base::OnceClosure callback) {
    on_access_token_request_callback_ = std::move(callback);
  }

 private:
  // OAuth2TokenService::DiagnosticsObserver:
  void OnAccessTokenRequested(
      const std::string& account_id,
      const std::string& consumer_id,
      const OAuth2TokenService::ScopeSet& scopes) override {
    if (on_access_token_request_callback_)
      std::move(on_access_token_request_callback_).Run();
  }

  base::MessageLoop message_loop_;
  TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;

  std::unique_ptr<AccountTrackerService> account_tracker_;
  base::OnceClosure on_access_token_request_callback_;
};

TEST_F(AccessTokenFetcherTest, CallsBackOnFulfilledRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      account_id, "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(AccessTokenFetcherTest, ShouldNotReplyIfDestroyed) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  // Destroy the fetcher before the access token request is fulfilled.
  fetcher.reset();

  // Now fulfilling the access token request should have no effect.
  token_service()->IssueAllTokensForAccount(
      account_id, "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(AccessTokenFetcherTest, ReturnsErrorWhenAccountIsUnknown) {
  TestTokenCallback callback;

  base::RunLoop run_loop;

  // Account not present -> we should get called back.
  auto fetcher = CreateFetcher("dummy_account_id", callback.Get());

  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError(
                      GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                  ""))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  run_loop.Run();
}

TEST_F(AccessTokenFetcherTest, ReturnsErrorWhenAccountHasNoRefreshToken) {
  TestTokenCallback callback;

  base::RunLoop run_loop;

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);

  // Account has no refresh token -> we should get called back.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError(
                      GoogleServiceAuthError::State::USER_NOT_SIGNED_UP),
                  ""))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  run_loop.Run();
}

TEST_F(AccessTokenFetcherTest, CanceledAccessTokenRequest) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  base::RunLoop run_loop2;
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED), ""))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop2, &base::RunLoop::Quit));

  // A canceled access token request should result in a callback.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  run_loop2.Run();
}

TEST_F(AccessTokenFetcherTest, RefreshTokenRevoked) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  TestTokenCallback callback;

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  // Simulate the refresh token getting invalidated. In this case, pending
  // access token requests get canceled, and the fetcher should *not* retry.
  token_service()->RevokeCredentials(account_id);
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          std::string()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

TEST_F(AccessTokenFetcherTest, FailedAccessTokenRequest) {
  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  TestTokenCallback callback;

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  // We should immediately get called back with an empty access token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
          std::string()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

TEST_F(AccessTokenFetcherTest, MultipleRequestsForSameAccountFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  // This should also result in a request for an access token.
  TestTokenCallback callback2;
  base::RunLoop run_loop2;
  set_on_access_token_request_callback(run_loop2.QuitClosure());
  auto fetcher2 = CreateFetcher(account_id, callback2.Get());
  run_loop2.Run();

  // Once the access token request is fulfilled, both requests should get
  // called back with the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  EXPECT_CALL(callback2,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      account_id, "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(AccessTokenFetcherTest, MultipleRequestsForDifferentAccountsFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());

  run_loop.Run();

  // Add a second account and request an access token for it.
  std::string account_id2 = AddAccount(kTestGaiaId2, kTestEmail2);
  token_service()->UpdateCredentials(account_id2, "refresh token");
  TestTokenCallback callback2;
  base::RunLoop run_loop2;
  set_on_access_token_request_callback(run_loop2.QuitClosure());
  auto fetcher2 = CreateFetcher(account_id2, callback2.Get());
  run_loop2.Run();

  // Once the first access token request is fulfilled, it should get
  // called back with the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      account_id, "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  // Once the second access token request is fulfilled, it should get
  // called back with the access token.
  EXPECT_CALL(callback2,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      account_id2, "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(AccessTokenFetcherTest,
       MultipleRequestsForDifferentAccountsCanceledAndFulfilled) {
  TestTokenCallback callback;

  base::RunLoop run_loop;
  set_on_access_token_request_callback(run_loop.QuitClosure());

  std::string account_id = AddAccount(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, "refresh token");

  // This should result in a request for an access token.
  auto fetcher = CreateFetcher(account_id, callback.Get());
  run_loop.Run();

  // Add a second account and request an access token for it.
  std::string account_id2 = AddAccount(kTestGaiaId2, kTestEmail2);
  token_service()->UpdateCredentials(account_id2, "refresh token");

  base::RunLoop run_loop2;
  set_on_access_token_request_callback(run_loop2.QuitClosure());

  TestTokenCallback callback2;
  auto fetcher2 = CreateFetcher(account_id2, callback2.Get());
  run_loop2.Run();

  // Cancel the first access token request: This should result in a callback
  // for the first fetcher.
  base::RunLoop run_loop3;
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED), ""))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop3, &base::RunLoop::Quit));

  token_service()->IssueErrorForAllPendingRequestsForAccount(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  run_loop3.Run();

  // Once the second access token request is fulfilled, it should get
  // called back with the access token.
  base::RunLoop run_loop4;
  EXPECT_CALL(callback2,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop4, &base::RunLoop::Quit));
  token_service()->IssueAllTokensForAccount(
      account_id2, "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));

  run_loop4.Run();
}

}  // namespace identity
