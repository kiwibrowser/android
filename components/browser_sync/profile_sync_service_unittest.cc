// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service.h"

#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/values.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/signin_manager_wrapper.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync/engine/fake_sync_engine.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info_values.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace browser_sync {

namespace {

class FakeDataTypeManager : public syncer::DataTypeManager {
 public:
  using ConfigureCalled =
      base::RepeatingCallback<void(syncer::ConfigureReason)>;

  explicit FakeDataTypeManager(const ConfigureCalled& configure_called)
      : configure_called_(configure_called) {}

  ~FakeDataTypeManager() override {}

  void Configure(syncer::ModelTypeSet desired_types,
                 syncer::ConfigureReason reason) override {
    DCHECK(!configure_called_.is_null());
    configure_called_.Run(reason);
  }

  void ReenableType(syncer::ModelType type) override {}
  void ResetDataTypeErrors() override {}
  void PurgeForMigration(syncer::ModelTypeSet undesired_types,
                         syncer::ConfigureReason reason) override {}
  void Stop(syncer::ShutdownReason reason) override {}
  syncer::ModelTypeSet GetActiveDataTypes() const override {
    return syncer::ModelTypeSet();
  }
  bool IsNigoriEnabled() const override { return true; }
  State state() const override { return syncer::DataTypeManager::CONFIGURED; }

 private:
  ConfigureCalled configure_called_;
};

ACTION_P(ReturnNewFakeDataTypeManager, configure_called) {
  return std::make_unique<FakeDataTypeManager>(configure_called);
}

class TestSyncServiceObserver : public syncer::SyncServiceObserver {
 public:
  TestSyncServiceObserver()
      : setup_in_progress_(false), auth_error_(GoogleServiceAuthError()) {}

  void OnStateChanged(syncer::SyncService* sync) override {
    setup_in_progress_ = sync->IsSetupInProgress();
    auth_error_ = sync->GetAuthError();
  }

  bool setup_in_progress() const { return setup_in_progress_; }
  GoogleServiceAuthError auth_error() const { return auth_error_; }

 private:
  bool setup_in_progress_;
  GoogleServiceAuthError auth_error_;
};

// A variant of the FakeSyncEngine that won't automatically call back when asked
// to initialized. Allows us to test things that could happen while backend init
// is in progress.
class FakeSyncEngineNoReturn : public syncer::FakeSyncEngine {
  void Initialize(InitParams params) override {}
};

// FakeSyncEngine that stores the SyncCredentials passed into Initialize(), and
// optionally also whether InvalidateCredentials was called.
class FakeSyncEngineCollectCredentials : public syncer::FakeSyncEngine {
 public:
  explicit FakeSyncEngineCollectCredentials(
      syncer::SyncCredentials* init_credentials,
      const base::RepeatingClosure& invalidate_credentials_callback)
      : init_credentials_(init_credentials),
        invalidate_credentials_callback_(invalidate_credentials_callback) {}

  void Initialize(InitParams params) override {
    *init_credentials_ = params.credentials;
    syncer::FakeSyncEngine::Initialize(std::move(params));
  }

  void InvalidateCredentials() override {
    if (invalidate_credentials_callback_) {
      invalidate_credentials_callback_.Run();
    }
    syncer::FakeSyncEngine::InvalidateCredentials();
  }

 private:
  syncer::SyncCredentials* init_credentials_;
  base::RepeatingClosure invalidate_credentials_callback_;
};

// FakeSyncEngine that calls an external callback when ClearServerData is
// called.
class FakeSyncEngineCaptureClearServerData : public syncer::FakeSyncEngine {
 public:
  using ClearServerDataCalled =
      base::RepeatingCallback<void(const base::Closure&)>;
  explicit FakeSyncEngineCaptureClearServerData(
      const ClearServerDataCalled& clear_server_data_called)
      : clear_server_data_called_(clear_server_data_called) {}

  void ClearServerData(const base::Closure& callback) override {
    clear_server_data_called_.Run(callback);
  }

 private:
  ClearServerDataCalled clear_server_data_called_;
};

ACTION(ReturnNewFakeSyncEngine) {
  return std::make_unique<syncer::FakeSyncEngine>();
}

void OnClearServerDataCalled(base::Closure* captured_callback,
                             const base::Closure& callback) {
  *captured_callback = callback;
}

// A test harness that uses a real ProfileSyncService and in most cases a
// MockSyncEngine.
//
// This is useful if we want to test the ProfileSyncService and don't care about
// testing the SyncEngine.
class ProfileSyncServiceTest : public ::testing::Test {
 protected:
  ProfileSyncServiceTest() {}
  ~ProfileSyncServiceTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSyncDeferredStartupTimeoutSeconds, "0");
  }

  void TearDown() override {
    // Kill the service before the profile.
    ShutdownAndDeleteService();
  }

  void SignIn() {
    identity::MakePrimaryAccountAvailable(signin_manager(), auth_service(),
                                          identity_manager(),
                                          "test_user@gmail.com");
  }

  void CreateService(ProfileSyncService::StartBehavior behavior) {
    DCHECK(!service_);

    ProfileSyncServiceBundle::SyncClientBuilder builder(
        &profile_sync_service_bundle_);
    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(behavior,
                                                           builder.Build());
    init_params.model_type_store_factory =
        syncer::ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest();

    service_ = std::make_unique<ProfileSyncService>(std::move(init_params));

    ON_CALL(*component_factory(), CreateCommonDataTypeControllers(_, _))
        .WillByDefault(testing::InvokeWithoutArgs([=]() {
          syncer::DataTypeController::TypeVector controllers;
          controllers.push_back(
              std::make_unique<syncer::FakeDataTypeController>(
                  syncer::BOOKMARKS));
          return controllers;
        }));
    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
        .WillByDefault(ReturnNewFakeSyncEngine());
    ON_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
        .WillByDefault(
            ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  }

  void CreateServiceWithLocalSyncBackend() {
    DCHECK(!service_);

    ProfileSyncServiceBundle::SyncClientBuilder builder(
        &profile_sync_service_bundle_);
    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(
            ProfileSyncService::AUTO_START, builder.Build());

    prefs()->SetBoolean(syncer::prefs::kEnableLocalSyncBackend, true);
    init_params.gaia_cookie_manager_service = nullptr;
    init_params.signin_wrapper.reset();

    service_ = std::make_unique<ProfileSyncService>(std::move(init_params));

    ON_CALL(*component_factory(), CreateCommonDataTypeControllers(_, _))
        .WillByDefault(testing::InvokeWithoutArgs([=]() {
          syncer::DataTypeController::TypeVector controllers;
          controllers.push_back(
              std::make_unique<syncer::FakeDataTypeController>(
                  syncer::BOOKMARKS));
          return controllers;
        }));
    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
        .WillByDefault(ReturnNewFakeSyncEngine());
    ON_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
        .WillByDefault(
            ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  }

  void ShutdownAndDeleteService() {
    if (service_)
      service_->Shutdown();
    service_.reset();
  }

  void InitializeForNthSync() {
    // Set first sync time before initialize to simulate a complete sync setup.
    syncer::SyncPrefs sync_prefs(prefs());
    sync_prefs.SetFirstSyncTime(base::Time::Now());
    sync_prefs.SetFirstSetupComplete();
    sync_prefs.SetKeepEverythingSynced(true);
    service_->Initialize();
  }

  void InitializeForFirstSync() { service_->Initialize(); }

  void TriggerPassphraseRequired() {
    service_->GetEncryptionObserverForTest()->OnPassphraseRequired(
        syncer::REASON_DECRYPTION, sync_pb::EncryptedData());
  }

  void TriggerDataTypeStartRequest() {
    service_->OnDataTypeRequestsSyncStartup(syncer::BOOKMARKS);
  }

  void OnConfigureCalled(syncer::ConfigureReason configure_reason) {
    syncer::DataTypeManager::ConfigureResult result;
    result.status = syncer::DataTypeManager::OK;
    if (configure_reason == syncer::CONFIGURE_REASON_CATCH_UP)
      result.was_catch_up_configure = true;
    service()->OnConfigureDone(result);
  }

  FakeDataTypeManager::ConfigureCalled GetDefaultConfigureCalledCallback() {
    return base::Bind(&ProfileSyncServiceTest::OnConfigureCalled,
                      base::Unretained(this));
  }

  FakeDataTypeManager::ConfigureCalled GetRecordingConfigureCalledCallback(
      syncer::ConfigureReason* reason_dest) {
    return base::BindLambdaForTesting(
        [reason_dest](syncer::ConfigureReason reason) {
          *reason_dest = reason;
        });
  }

  AccountTrackerService* account_tracker() {
    return profile_sync_service_bundle_.account_tracker();
  }

#if defined(OS_CHROMEOS)
  FakeSigninManagerBase* signin_manager()
#else
  FakeSigninManager* signin_manager()
#endif
  // Opening brace is outside of macro to avoid confusing lint.
  {
    return profile_sync_service_bundle_.signin_manager();
  }

  FakeProfileOAuth2TokenService* auth_service() {
    return profile_sync_service_bundle_.auth_service();
  }

  identity::IdentityManager* identity_manager() {
    return profile_sync_service_bundle_.identity_manager();
  }

  ProfileSyncService* service() { return service_.get(); }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_sync_service_bundle_.pref_service();
  }

  syncer::SyncApiComponentFactoryMock* component_factory() {
    return profile_sync_service_bundle_.component_factory();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ProfileSyncServiceBundle profile_sync_service_bundle_;
  std::unique_ptr<ProfileSyncService> service_;
};

// Verify that the server URLs are sane.
TEST_F(ProfileSyncServiceTest, InitialState) {
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  const std::string& url = service()->sync_service_url().spec();
  EXPECT_TRUE(url == syncer::internal::kSyncServerUrl ||
              url == syncer::internal::kSyncDevServerUrl);
}

// Verify a successful initialization.
TEST_F(ProfileSyncServiceTest, SuccessfulInitialization) {
  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(ReturnNewFakeSyncEngine());
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->IsSyncActive());
}

// Verify a successful initialization.
TEST_F(ProfileSyncServiceTest, SuccessfulLocalBackendInitialization) {
  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  CreateServiceWithLocalSyncBackend();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(ReturnNewFakeSyncEngine());
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(service()->IsSyncConfirmationNeeded());
}

// Verify that an initialization where first setup is not complete does not
// start up the backend.
TEST_F(ProfileSyncServiceTest, NeedsConfirmation) {
  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  SignIn();
  CreateService(ProfileSyncService::MANUAL_START);

  syncer::SyncPrefs sync_prefs(prefs());
  base::Time now = base::Time::Now();
  sync_prefs.SetLastSyncedTime(now);
  sync_prefs.SetKeepEverythingSynced(true);
  service()->Initialize();
  EXPECT_FALSE(service()->IsSyncActive());
  EXPECT_TRUE(service()->IsSyncConfirmationNeeded());

  // The last sync time shouldn't be cleared.
  // TODO(zea): figure out a way to check that the directory itself wasn't
  // cleared.
  EXPECT_EQ(now, sync_prefs.GetLastSyncedTime());
}

// Verify that the SetSetupInProgress function call updates state
// and notifies observers.
TEST_F(ProfileSyncServiceTest, SetupInProgress) {
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForFirstSync();

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  auto sync_blocker = service()->GetSetupInProgressHandle();
  EXPECT_TRUE(observer.setup_in_progress());
  sync_blocker.reset();
  EXPECT_FALSE(observer.setup_in_progress());

  service()->RemoveObserver(&observer);
}

// Verify that disable by enterprise policy works.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyBeforeInit) {
  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->IsSyncActive());
}

// Verify that disable by enterprise policy works even after the backend has
// been initialized.
TEST_F(ProfileSyncServiceTest, DisabledByPolicyAfterInit) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();

  ASSERT_FALSE(service()->IsManaged());
  ASSERT_TRUE(service()->IsSyncActive());

  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->IsSyncActive());
}

// Exercises the ProfileSyncService's code paths related to getting shut down
// before the backend initialize call returns.
TEST_F(ProfileSyncServiceTest, AbortedByShutdown) {
  CreateService(ProfileSyncService::AUTO_START);
  ON_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillByDefault(
          Return(ByMove(std::make_unique<FakeSyncEngineNoReturn>())));

  SignIn();
  InitializeForNthSync();
  ASSERT_FALSE(service()->IsSyncActive());

  ShutdownAndDeleteService();
}

// Test RequestStop() before we've initialized the backend.
TEST_F(ProfileSyncServiceTest, EarlyRequestStop) {
  CreateService(ProfileSyncService::AUTO_START);
  SignIn();

  service()->RequestStop(ProfileSyncService::KEEP_DATA);
  EXPECT_FALSE(service()->IsSyncRequested());

  // Because sync is not requested, this should fail.
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsSyncRequested());
  EXPECT_FALSE(service()->IsSyncActive());

  // Request start. This should be enough to allow init to happen.
  service()->RequestStart();
  EXPECT_TRUE(service()->IsSyncRequested());
  EXPECT_TRUE(service()->IsSyncActive());
}

// Test RequestStop() after we've initialized the backend.
TEST_F(ProfileSyncServiceTest, DisableAndEnableSyncTemporarily) {
  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  InitializeForNthSync();

  ASSERT_TRUE(service()->IsSyncActive());
  ASSERT_FALSE(prefs()->GetBoolean(syncer::prefs::kSyncSuppressStart));

  testing::Mock::VerifyAndClearExpectations(component_factory());

  service()->RequestStop(ProfileSyncService::KEEP_DATA);
  EXPECT_FALSE(service()->IsSyncActive());
  EXPECT_TRUE(prefs()->GetBoolean(syncer::prefs::kSyncSuppressStart));

  service()->RequestStart();
  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(syncer::prefs::kSyncSuppressStart));
}

// Certain ProfileSyncService tests don't apply to Chrome OS, for example
// things that deal with concepts like "signing out" and policy.
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, EnableSyncAndSignOut) {
  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  InitializeForNthSync();

  EXPECT_TRUE(service()->IsSyncActive());
  EXPECT_FALSE(prefs()->GetBoolean(syncer::prefs::kSyncSuppressStart));

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  // Wait for PSS to be notified that the primary account has gone away.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->IsSyncActive());
}
#endif  // !defined(OS_CHROMEOS)

TEST_F(ProfileSyncServiceTest, GetSyncTokenStatus) {
  CreateService(ProfileSyncService::AUTO_START);

  SignIn();
  InitializeForNthSync();

  // Initial status.
  syncer::SyncTokenStatus token_status = service()->GetSyncTokenStatus();
  ASSERT_EQ(syncer::CONNECTION_NOT_ATTEMPTED, token_status.connection_status);
  ASSERT_TRUE(token_status.connection_status_update_time.is_null());
  ASSERT_TRUE(token_status.token_request_time.is_null());
  ASSERT_TRUE(token_status.token_receive_time.is_null());

  // Simulate an auth error.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  // The token request will take the form of a posted task.  Run it.
  base::RunLoop().RunUntilIdle();

  token_status = service()->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_AUTH_ERROR, token_status.connection_status);
  EXPECT_FALSE(token_status.connection_status_update_time.is_null());
  EXPECT_FALSE(token_status.token_request_time.is_null());
  EXPECT_FALSE(token_status.token_receive_time.is_null());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            token_status.last_get_token_error);
  EXPECT_TRUE(token_status.next_token_request_time.is_null());

  // Simulate successful connection.
  service()->OnConnectionStatusChange(syncer::CONNECTION_OK);
  token_status = service()->GetSyncTokenStatus();
  EXPECT_EQ(syncer::CONNECTION_OK, token_status.connection_status);
}

TEST_F(ProfileSyncServiceTest, RevokeAccessTokenFromTokenService) {
  syncer::SyncCredentials init_credentials;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_credentials, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());

  const std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();

  // Make sure the expected credentials (correct account_id, empty access token)
  // were passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_credentials.account_id);
  ASSERT_TRUE(init_credentials.sync_token.empty());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());

  std::string secondary_account_gaiaid = "1234567";
  std::string secondary_account_name = "test_user2@gmail.com";
  std::string secondary_account_id = account_tracker()->SeedAccountInfo(
      secondary_account_gaiaid, secondary_account_name);
  auth_service()->UpdateCredentials(secondary_account_id,
                                    "second_account_refresh_token");
  auth_service()->RevokeCredentials(secondary_account_id);
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  auth_service()->RevokeCredentials(primary_account_id);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}

// Checks that CREDENTIALS_REJECTED_BY_CLIENT resets the access token and stops
// Sync. Regression test for https://crbug.com/824791.
TEST_F(ProfileSyncServiceTest, CredentialsRejectedByClient) {
  syncer::SyncCredentials init_credentials;
  bool invalidate_credentials_called = false;
  base::RepeatingClosure invalidate_credentials_callback =
      base::BindRepeating([](bool* called) { *called = true; },
                          base::Unretained(&invalidate_credentials_called));

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_credentials, invalidate_credentials_callback))));
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  const std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();

  // Make sure the expected credentials (correct account_id, empty access token)
  // were passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_credentials.account_id);
  ASSERT_TRUE(init_credentials.sync_token.empty());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), service()->GetAuthError());
  ASSERT_EQ(GoogleServiceAuthError::AuthErrorNone(), observer.auth_error());

  // Simulate the credentials getting locally rejected by the client by setting
  // the refresh token to a special invalid value.
  auth_service()->UpdateCredentials(
      primary_account_id, OAuth2TokenServiceDelegate::kInvalidRefreshToken);
  GoogleServiceAuthError rejected_by_client =
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_CLIENT);
  ASSERT_EQ(rejected_by_client,
            auth_service()->GetAuthError(primary_account_id));
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
  EXPECT_TRUE(invalidate_credentials_called);

  // The observer should have been notified of the auth error state.
  EXPECT_EQ(rejected_by_client, observer.auth_error());

  service()->RemoveObserver(&observer);
}

// CrOS does not support signout.
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceTest, SignOutRevokeAccessToken) {
  syncer::SyncCredentials init_credentials;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_credentials, base::RepeatingClosure()))));
  InitializeForNthSync();
  EXPECT_TRUE(service()->IsSyncActive());

  const std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();

  // Make sure the expected credentials (correct account_id, empty access token)
  // were passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_credentials.account_id);
  ASSERT_TRUE(init_credentials.sync_token.empty());

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(service()->GetAccessTokenForTest().empty());

  signin_manager()->SignOut(signin_metrics::SIGNOUT_TEST,
                            signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_TRUE(service()->GetAccessTokenForTest().empty());
}
#endif

// Verify that LastSyncedTime and local DeviceInfo is cleared on sign out.
TEST_F(ProfileSyncServiceTest, ClearDataOnSignOut) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());
  ASSERT_LT(base::Time::Now() - service()->GetLastSyncedTime(),
            base::TimeDelta::FromMinutes(1));
  ASSERT_TRUE(service()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo());

  // Sign out.
  service()->RequestStop(ProfileSyncService::CLEAR_DATA);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(service()->GetLastSyncedTime().is_null());
  EXPECT_FALSE(service()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo());
}

// Verify that credential errors get returned from GetAuthError().
TEST_F(ProfileSyncServiceTest, CredentialErrorReturned) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  auth_service()->set_auto_post_fetch_response_on_message_loop(false);

  syncer::SyncCredentials init_credentials;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_credentials, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());

  const std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();

  // Make sure the expected credentials (correct account_id, empty access token)
  // were passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_credentials.account_id);
  ASSERT_TRUE(init_credentials.sync_token.empty());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  // Wait for ProfileSyncService to send an access token request.
  base::RunLoop().RunUntilIdle();
  auth_service()->IssueAllTokensForAccount(primary_account_id, "access token",
                                           base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  auth_service()->UpdateCredentials(primary_account_id, "not a valid token");
  // Again, wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  auth_service()->IssueErrorForAllPendingRequests(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            observer.auth_error().state());

  service()->RemoveObserver(&observer);
}

// Verify that credential errors get cleared when a new token is fetched
// successfully.
TEST_F(ProfileSyncServiceTest, CredentialErrorClearsOnNewToken) {
  // This test needs to manually send access tokens (or errors), so disable
  // automatic replies to access token requests.
  auth_service()->set_auto_post_fetch_response_on_message_loop(false);

  syncer::SyncCredentials init_credentials;

  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCollectCredentials>(
              &init_credentials, base::RepeatingClosure()))));
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());

  const std::string primary_account_id =
      signin_manager()->GetAuthenticatedAccountId();

  // Make sure the expected credentials (correct account_id, empty access token)
  // were passed to the SyncEngine.
  ASSERT_EQ(primary_account_id, init_credentials.account_id);
  ASSERT_TRUE(init_credentials.sync_token.empty());

  TestSyncServiceObserver observer;
  service()->AddObserver(&observer);

  // At this point, the real SyncEngine would try to connect to the server, fail
  // (because it has no access token), and eventually call
  // OnConnectionStatusChange(CONNECTION_AUTH_ERROR). Since our fake SyncEngine
  // doesn't do any of this, call that explicitly here.
  service()->OnConnectionStatusChange(syncer::CONNECTION_AUTH_ERROR);

  // Wait for ProfileSyncService to send an access token request.
  base::RunLoop().RunUntilIdle();
  auth_service()->IssueAllTokensForAccount(primary_account_id, "access token",
                                           base::Time::Max());
  ASSERT_FALSE(service()->GetAccessTokenForTest().empty());
  ASSERT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());

  // Emulate Chrome receiving a new, invalid LST. This happens when the user
  // signs out of the content area.
  auth_service()->UpdateCredentials(primary_account_id, "not a valid token");
  // Wait for ProfileSyncService to be notified of the changed credentials and
  // send a new access token request.
  base::RunLoop().RunUntilIdle();
  auth_service()->IssueErrorForAllPendingRequests(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Check that the invalid token is returned from sync.
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            service()->GetAuthError().state());

  // Now emulate Chrome receiving a new, valid LST.
  auth_service()->UpdateCredentials(primary_account_id, "totally valid token");
  // Again, wait for ProfileSyncService to be notified.
  base::RunLoop().RunUntilIdle();
  auth_service()->IssueTokenForAllPendingRequests(
      "this one works", base::Time::Now() + base::TimeDelta::FromDays(10));

  // Check that sync auth error state cleared.
  EXPECT_EQ(GoogleServiceAuthError::NONE, service()->GetAuthError().state());
  EXPECT_EQ(GoogleServiceAuthError::NONE, observer.auth_error().state());

  service()->RemoveObserver(&observer);
}

// Verify that the disable sync flag disables sync.
TEST_F(ProfileSyncServiceTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableSync);
  EXPECT_FALSE(ProfileSyncService::IsSyncAllowedByFlag());
}

// Verify that no disable sync flag enables sync.
TEST_F(ProfileSyncServiceTest, NoDisableSyncFlag) {
  EXPECT_TRUE(ProfileSyncService::IsSyncAllowedByFlag());
}

// Test Sync will stop after receive memory pressure
TEST_F(ProfileSyncServiceTest, MemoryPressureRecording) {
  CreateService(ProfileSyncService::AUTO_START);
  SignIn();
  InitializeForNthSync();

  ASSERT_TRUE(service()->IsSyncActive());
  ASSERT_FALSE(prefs()->GetBoolean(syncer::prefs::kSyncSuppressStart));

  testing::Mock::VerifyAndClearExpectations(component_factory());

  syncer::SyncPrefs sync_prefs(service()->GetSyncClient()->GetPrefService());

  ASSERT_EQ(prefs()->GetInteger(syncer::prefs::kSyncMemoryPressureWarningCount),
            0);
  ASSERT_FALSE(sync_prefs.DidSyncShutdownCleanly());

  // Simulate memory pressure notification.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  // Verify memory pressure recorded.
  EXPECT_EQ(prefs()->GetInteger(syncer::prefs::kSyncMemoryPressureWarningCount),
            1);
  EXPECT_FALSE(sync_prefs.DidSyncShutdownCleanly());

  // Simulate memory pressure notification.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  ShutdownAndDeleteService();

  // Verify memory pressure and shutdown recorded.
  EXPECT_EQ(prefs()->GetInteger(syncer::prefs::kSyncMemoryPressureWarningCount),
            2);
  EXPECT_TRUE(sync_prefs.DidSyncShutdownCleanly());
}

// Verify that OnLocalSetPassphraseEncryption triggers catch up configure sync
// cycle, calls ClearServerData, shuts down and restarts sync.
TEST_F(ProfileSyncServiceTest, OnLocalSetPassphraseEncryption) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kSyncClearDataOnPassphraseEncryption);
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);

  base::Closure captured_callback;
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;

  // Initialize sync, ensure that both DataTypeManager and SyncEngine are
  // initialized and DTM::Configure is called with
  // CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE.
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCaptureClearServerData>(
              base::BindRepeating(&OnClearServerDataCalled,
                                  base::Unretained(&captured_callback))))));
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());
  testing::Mock::VerifyAndClearExpectations(component_factory());
  ASSERT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  syncer::DataTypeManager::ConfigureResult result;
  result.status = syncer::DataTypeManager::OK;
  service()->OnConfigureDone(result);

  // Simulate user entering encryption passphrase. Ensure that catch up
  // configure cycle is started (DTM::Configure is called with
  // CONFIGURE_REASON_CATCH_UP).
  const syncer::SyncEncryptionHandler::NigoriState nigori_state;
  service()->GetEncryptionObserverForTest()->OnLocalSetPassphraseEncryption(
      nigori_state);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate configure successful. Ensure that SBH::ClearServerData is called.
  result.was_catch_up_configure = true;
  service()->OnConfigureDone(result);
  result.was_catch_up_configure = false;
  EXPECT_FALSE(captured_callback.is_null());

  // Once SBH::ClearServerData finishes successfully ensure that sync is
  // restarted.
  configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  captured_callback.Run();
  testing::Mock::VerifyAndClearExpectations(component_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  service()->OnConfigureDone(result);
}

// Verify that if after OnLocalSetPassphraseEncryption catch up configure sync
// cycle gets interrupted, it starts again after browser restart.
TEST_F(ProfileSyncServiceTest,
       OnLocalSetPassphraseEncryption_RestartDuringCatchUp) {
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kSyncClearDataOnPassphraseEncryption);
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForNthSync();
  testing::Mock::VerifyAndClearExpectations(component_factory());
  ASSERT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  syncer::DataTypeManager::ConfigureResult result;
  result.status = syncer::DataTypeManager::OK;
  service()->OnConfigureDone(result);

  // Simulate user entering encryption passphrase. Ensure Configure was called
  // but don't let it continue.
  const syncer::SyncEncryptionHandler::NigoriState nigori_state;
  service()->GetEncryptionObserverForTest()->OnLocalSetPassphraseEncryption(
      nigori_state);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);

  // Simulate browser restart. First configuration is a regular one.
  ShutdownAndDeleteService();
  CreateService(ProfileSyncService::AUTO_START);
  base::Closure captured_callback;
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCaptureClearServerData>(
              base::BindRepeating(&OnClearServerDataCalled,
                                  base::Unretained(&captured_callback))))));
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForNthSync();
  testing::Mock::VerifyAndClearExpectations(component_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate configure successful. This time it should be catch up.
  service()->OnConfigureDone(result);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate catch up configure successful. Ensure that SBH::ClearServerData is
  // called.
  result.was_catch_up_configure = true;
  service()->OnConfigureDone(result);
  result.was_catch_up_configure = false;
  EXPECT_FALSE(captured_callback.is_null());

  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  captured_callback.Run();
  testing::Mock::VerifyAndClearExpectations(component_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
}

// Verify that if after OnLocalSetPassphraseEncryption ClearServerData gets
// interrupted, transition again from catch up sync cycle after browser restart.
TEST_F(ProfileSyncServiceTest,
       OnLocalSetPassphraseEncryption_RestartDuringClearServerData) {
  base::Closure captured_callback;
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      switches::kSyncClearDataOnPassphraseEncryption);
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCaptureClearServerData>(
              base::BindRepeating(&OnClearServerDataCalled,
                                  base::Unretained(&captured_callback))))));
  InitializeForNthSync();
  testing::Mock::VerifyAndClearExpectations(component_factory());

  // Simulate user entering encryption passphrase.
  const syncer::SyncEncryptionHandler::NigoriState nigori_state;
  service()->GetEncryptionObserverForTest()->OnLocalSetPassphraseEncryption(
      nigori_state);
  EXPECT_FALSE(captured_callback.is_null());
  captured_callback.Reset();

  // Simulate browser restart. First configuration is a regular one.
  ShutdownAndDeleteService();
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(
          Return(ByMove(std::make_unique<FakeSyncEngineCaptureClearServerData>(
              base::BindRepeating(&OnClearServerDataCalled,
                                  base::Unretained(&captured_callback))))));
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForNthSync();
  testing::Mock::VerifyAndClearExpectations(component_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate configure successful. This time it should be catch up.
  syncer::DataTypeManager::ConfigureResult result;
  result.status = syncer::DataTypeManager::OK;
  service()->OnConfigureDone(result);
  EXPECT_EQ(syncer::CONFIGURE_REASON_CATCH_UP, configure_reason);
  EXPECT_TRUE(captured_callback.is_null());

  // Simulate catch up configure successful. Ensure that SBH::ClearServerData is
  // called.
  result.was_catch_up_configure = true;
  service()->OnConfigureDone(result);
  result.was_catch_up_configure = false;
  EXPECT_FALSE(captured_callback.is_null());

  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  captured_callback.Run();
  testing::Mock::VerifyAndClearExpectations(component_factory());
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
}

// Test that the passphrase prompt due to version change logic gets triggered
// on a datatype type requesting startup, but only happens once.
TEST_F(ProfileSyncServiceTest, PassphrasePromptDueToVersion) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();

  syncer::SyncPrefs sync_prefs(service()->GetSyncClient()->GetPrefService());
  ASSERT_EQ(PRODUCT_VERSION, sync_prefs.GetLastRunVersion());

  sync_prefs.SetPassphrasePrompted(true);

  // Until a datatype requests startup while a passphrase is required the
  // passphrase prompt bit should remain set.
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());
  TriggerPassphraseRequired();
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());

  // Because the last version was unset, this run should be treated as a new
  // version and force a prompt.
  TriggerDataTypeStartRequest();
  EXPECT_FALSE(sync_prefs.IsPassphrasePrompted());

  // At this point further datatype startup request should have no effect.
  sync_prefs.SetPassphrasePrompted(true);
  TriggerDataTypeStartRequest();
  EXPECT_TRUE(sync_prefs.IsPassphrasePrompted());
}

// Test that when ProfileSyncService receives actionable error
// RESET_LOCAL_SYNC_DATA it restarts sync.
TEST_F(ProfileSyncServiceTest, ResetSyncData) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  // Backend should get initialized two times: once during initialization and
  // once when handling actionable error.
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()))
      .WillOnce(
          ReturnNewFakeDataTypeManager(GetDefaultConfigureCalledCallback()));
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
      .WillOnce(ReturnNewFakeSyncEngine())
      .WillOnce(ReturnNewFakeSyncEngine());

  InitializeForNthSync();

  syncer::SyncProtocolError client_cmd;
  client_cmd.action = syncer::RESET_LOCAL_SYNC_DATA;
  service()->OnActionableError(client_cmd);
}

// Test that when ProfileSyncService receives actionable error
// DISABLE_SYNC_ON_CLIENT it disables sync and signs out.
TEST_F(ProfileSyncServiceTest, DisableSyncOnClient) {
  SignIn();
  CreateService(ProfileSyncService::AUTO_START);
  InitializeForNthSync();

  ASSERT_TRUE(service()->IsSyncActive());
  ASSERT_LT(base::Time::Now() - service()->GetLastSyncedTime(),
            base::TimeDelta::FromMinutes(1));
  ASSERT_TRUE(service()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo());

  syncer::SyncProtocolError client_cmd;
  client_cmd.action = syncer::DISABLE_SYNC_ON_CLIENT;
  service()->OnActionableError(client_cmd);

// CrOS does not support signout.
#if !defined(OS_CHROMEOS)
  EXPECT_TRUE(signin_manager()->GetAuthenticatedAccountId().empty());
#else
  EXPECT_FALSE(signin_manager()->GetAuthenticatedAccountId().empty());
#endif

  EXPECT_FALSE(service()->IsSyncActive());
  EXPECT_TRUE(service()->GetLastSyncedTime().is_null());
  EXPECT_FALSE(service()->GetLocalDeviceInfoProvider()->GetLocalDeviceInfo());
}

// Verify a that local sync mode resumes after the policy is lifted.
TEST_F(ProfileSyncServiceTest, LocalBackendDisabledByPolicy) {
  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));
  CreateServiceWithLocalSyncBackend();
  InitializeForNthSync();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->IsSyncActive());

  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(true));

  EXPECT_TRUE(service()->IsManaged());
  EXPECT_FALSE(service()->IsSyncActive());

  prefs()->SetManagedPref(syncer::prefs::kSyncManaged,
                          std::make_unique<base::Value>(false));

  service()->RequestStart();
  EXPECT_FALSE(service()->IsManaged());
  EXPECT_TRUE(service()->IsSyncActive());
}

// Test ConfigureDataTypeManagerReason on First and Nth start.
TEST_F(ProfileSyncServiceTest, ConfigureDataTypeManagerReason) {
  const syncer::DataTypeManager::ConfigureResult configure_result(
      syncer::DataTypeManager::OK, syncer::ModelTypeSet());
  syncer::ConfigureReason configure_reason = syncer::CONFIGURE_REASON_UNKNOWN;

  SignIn();

  // First sync.
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForFirstSync();
  ASSERT_TRUE(service()->IsSyncActive());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(component_factory()));
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEW_CLIENT, configure_reason);
  service()->OnConfigureDone(configure_result);

  // Reconfiguration.
  service()->ReconfigureDatatypeManager();
  EXPECT_EQ(syncer::CONFIGURE_REASON_RECONFIGURATION, configure_reason);
  service()->OnConfigureDone(configure_result);
  ShutdownAndDeleteService();

  // Nth sync.
  CreateService(ProfileSyncService::AUTO_START);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .WillOnce(ReturnNewFakeDataTypeManager(
          GetRecordingConfigureCalledCallback(&configure_reason)));
  InitializeForNthSync();
  ASSERT_TRUE(service()->IsSyncActive());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(component_factory()));
  EXPECT_EQ(syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE, configure_reason);
  service()->OnConfigureDone(configure_result);

  // Reconfiguration.
  service()->ReconfigureDatatypeManager();
  EXPECT_EQ(syncer::CONFIGURE_REASON_RECONFIGURATION, configure_reason);
  service()->OnConfigureDone(configure_result);
  ShutdownAndDeleteService();
}

}  // namespace
}  // namespace browser_sync
