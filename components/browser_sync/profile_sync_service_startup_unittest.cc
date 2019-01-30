// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_service.h"

#include "base/test/scoped_task_environment.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/data_type_manager_mock.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/engine/fake_sync_engine.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::DataTypeManager;
using syncer::DataTypeManagerMock;
using syncer::FakeSyncEngine;
using testing::_;
using testing::ByMove;
using testing::DoAll;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace browser_sync {

namespace {

const char kEmail[] = "test_user@gmail.com";

void SetError(DataTypeManager::ConfigureResult* result) {
  syncer::DataTypeStatusTable::TypeErrorMap errors;
  errors[syncer::BOOKMARKS] =
      syncer::SyncError(FROM_HERE, syncer::SyncError::UNRECOVERABLE_ERROR,
                        "Error", syncer::BOOKMARKS);
  result->data_type_status_table.UpdateFailedDataTypes(errors);
}

}  // namespace

ACTION_P(InvokeOnConfigureStart, sync_service) {
  sync_service->OnConfigureStart();
}

ACTION_P3(InvokeOnConfigureDone, sync_service, error_callback, result) {
  DataTypeManager::ConfigureResult configure_result =
      static_cast<DataTypeManager::ConfigureResult>(result);
  if (result.status == syncer::DataTypeManager::ABORTED)
    error_callback.Run(&configure_result);
  sync_service->OnConfigureDone(configure_result);
}

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest() {
    profile_sync_service_bundle_.auth_service()
        ->set_auto_post_fetch_response_on_message_loop(true);
  }

  ~ProfileSyncServiceStartupTest() override {
    sync_service_->Shutdown();
  }

  void CreateSyncService(ProfileSyncService::StartBehavior start_behavior) {
    ProfileSyncServiceBundle::SyncClientBuilder builder(
        &profile_sync_service_bundle_);
    ProfileSyncService::InitParams init_params =
        profile_sync_service_bundle_.CreateBasicInitParams(start_behavior,
                                                           builder.Build());

    ON_CALL(*component_factory(), CreateCommonDataTypeControllers(_, _))
        .WillByDefault(testing::InvokeWithoutArgs([=]() {
          syncer::DataTypeController::TypeVector controllers;
          controllers.push_back(
              std::make_unique<syncer::FakeDataTypeController>(
                  syncer::BOOKMARKS));
          return controllers;
        }));

    sync_service_ =
        std::make_unique<ProfileSyncService>(std::move(init_params));
  }

  void SimulateTestUserSignin() {
    identity::MakePrimaryAccountAvailable(
        profile_sync_service_bundle_.signin_manager(),
        profile_sync_service_bundle_.auth_service(),
        profile_sync_service_bundle_.identity_manager(), kEmail);
  }

  void SimulateTestUserSigninWithoutRefreshToken() {
    // Set the primary account *without* providing an OAuth token.
    identity::SetPrimaryAccount(profile_sync_service_bundle_.signin_manager(),
                                profile_sync_service_bundle_.identity_manager(),
                                kEmail);
  }

  void UpdateCredentials() {
    identity::SetRefreshTokenForPrimaryAccount(
        profile_sync_service_bundle_.auth_service(),
        profile_sync_service_bundle_.identity_manager());
  }

  DataTypeManagerMock* SetUpDataTypeManagerMock() {
    auto data_type_manager = std::make_unique<NiceMock<DataTypeManagerMock>>();
    DataTypeManagerMock* data_type_manager_raw = data_type_manager.get();
    ON_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
        .WillByDefault(Return(ByMove(std::move(data_type_manager))));
    return data_type_manager_raw;
  }

  FakeSyncEngine* SetUpFakeSyncEngine() {
    auto sync_engine = std::make_unique<FakeSyncEngine>();
    FakeSyncEngine* sync_engine_raw = sync_engine.get();
    ON_CALL(*component_factory(), CreateSyncEngine(_, _, _, _))
        .WillByDefault(Return(ByMove(std::move(sync_engine))));
    return sync_engine_raw;
  }

  ProfileSyncService* sync_service() { return sync_service_.get(); }

  PrefService* pref_service() {
    return profile_sync_service_bundle_.pref_service();
  }

  syncer::SyncApiComponentFactoryMock* component_factory() {
    return profile_sync_service_bundle_.component_factory();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ProfileSyncServiceBundle profile_sync_service_bundle_;
  std::unique_ptr<ProfileSyncService> sync_service_;
  syncer::DataTypeStatusTable data_type_status_table_;
};

class ProfileSyncServiceStartupCrosTest : public ProfileSyncServiceStartupTest {
 public:
  ProfileSyncServiceStartupCrosTest() {
    CreateSyncService(ProfileSyncService::AUTO_START);
    SimulateTestUserSigninWithoutRefreshToken();
  }
};

// ChromeOS does not support sign-in after startup (in particular,
// IdentityManager::Observer::OnPrimaryAccountSet never gets called).
#if !defined(OS_CHROMEOS)
TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  // We've never completed startup.
  pref_service()->ClearPref(syncer::prefs::kSyncFirstSetupComplete);
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::STOPPED));

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  sync_service()->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, pref_service()->GetInt64(syncer::prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(
      pref_service()->GetBoolean(syncer::prefs::kSyncFirstSetupComplete));

  // Confirmation isn't needed before sign in occurs.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());
  EXPECT_FALSE(sync_service()->IsSyncActive());

  // This tells the ProfileSyncService that setup is now in progress, which
  // causes it to try starting up the engine. We're not signed in yet though, so
  // that won't work.
  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  EXPECT_FALSE(sync_service()->IsSyncActive());

  EXPECT_FALSE(sync_service()->IsEngineInitialized());

  // Confirmation isn't needed before sign in occurs, or when setup is already
  // in progress.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  // Simulate successful signin as test_user. This will cause ProfileSyncService
  // to start, since all conditions are now fulfilled.
  SimulateTestUserSignin();

  // Now we're signed in, so the engine can start.
  EXPECT_TRUE(sync_service()->IsEngineInitialized());

  // Sync itself still isn't active though while setup is in progress.
  EXPECT_FALSE(sync_service()->IsSyncActive());

  // Setup is already in progress, so confirmation still isn't needed.
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  // Releasing the sync blocker will let ProfileSyncService configure the
  // DataTypeManager.
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  // Simulate the UI telling sync it has finished setting up.
  sync_blocker.reset();
  sync_service()->SetFirstSetupComplete();

  // This should have enabled sync.
  EXPECT_TRUE(sync_service()->IsSyncActive());
  EXPECT_FALSE(sync_service()->IsSyncConfirmationNeeded());

  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
}
#endif  // OS_CHROMEOS

TEST_F(ProfileSyncServiceStartupTest, StartNoCredentials) {
  // We're already signed in, but don't have a refresh token.
  SimulateTestUserSigninWithoutRefreshToken();

  CreateSyncService(ProfileSyncService::MANUAL_START);

  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  sync_service()->Initialize();

  // ProfileSyncService should now be active, but of course not have an access
  // token.
  EXPECT_TRUE(sync_service()->IsSyncActive());
  EXPECT_TRUE(sync_service()->GetAccessTokenForTest().empty());
  // Note that ProfileSyncService is not in an auth error state - no auth was
  // attempted, so no error.
}

TEST_F(ProfileSyncServiceStartupTest, StartInvalidCredentials) {
  SimulateTestUserSignin();

  CreateSyncService(ProfileSyncService::MANUAL_START);

  sync_service()->SetFirstSetupComplete();

  // Tell the engine to stall while downloading control types (simulating an
  // auth error).
  FakeSyncEngine* fake_engine = SetUpFakeSyncEngine();
  fake_engine->set_fail_initial_download(true);
  // Note: Since engine initialization will fail, the DataTypeManager should not
  // get created at all here.

  sync_service()->Initialize();

  EXPECT_FALSE(sync_service()->IsSyncActive());
  // Engine initialization failures puts the service into an unrecoverable error
  // state. It'll take either a browser restart or a full sign-out+sign-in to
  // get out of this.
  EXPECT_TRUE(sync_service()->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartCrosNoCredentials) {
  pref_service()->ClearPref(syncer::prefs::kSyncFirstSetupComplete);

  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();

  EXPECT_CALL(*data_type_manager, Configure(_, _));
  sync_service()->Initialize();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  // Sync should start up the engine, even though there is no refresh token yet.
  EXPECT_TRUE(sync_service()->IsSyncActive());
  // Since we're in AUTO_START mode, FirstSetupComplete gets set automatically.
  EXPECT_TRUE(sync_service()->IsFirstSetupComplete());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartFirstTime) {
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  pref_service()->ClearPref(syncer::prefs::kSyncFirstSetupComplete);
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));

  // The primary account is already populated, all that's left to do is provide
  // a refresh token.
  UpdateCredentials();
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->IsSyncActive());
  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
}

TEST_F(ProfileSyncServiceStartupTest, StartNormal) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();
  EXPECT_CALL(*data_type_manager, Stop(syncer::BROWSER_SHUTDOWN));
}

TEST_F(ProfileSyncServiceStartupTest, StopSync) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_CALL(*data_type_manager, Stop(syncer::STOP_SYNC));
  sync_service()->RequestStop(syncer::SyncService::KEEP_DATA);
}

TEST_F(ProfileSyncServiceStartupTest, DisableSync) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_CALL(*data_type_manager, Stop(syncer::DISABLE_SYNC));
  sync_service()->RequestStop(syncer::SyncService::CLEAR_DATA);
}

// Test that we can recover from a case where a bug in the code resulted in
// OnUserChoseDatatypes not being properly called and datatype preferences
// therefore being left unset.
TEST_F(ProfileSyncServiceStartupTest, StartRecoverDatatypePrefs) {
  // Clear the datatype preference fields (simulating bug 154940).
  pref_service()->ClearPref(syncer::prefs::kSyncKeepEverythingSynced);
  syncer::ModelTypeSet user_types = syncer::UserTypes();
  for (syncer::ModelTypeSet::Iterator iter = user_types.First(); iter.Good();
       iter.Inc()) {
    pref_service()->ClearPref(
        syncer::SyncPrefs::GetPrefNameForDataType(iter.Get()));
  }

  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));

  sync_service()->Initialize();

  EXPECT_TRUE(
      pref_service()->GetBoolean(syncer::prefs::kSyncKeepEverythingSynced));
}

// Verify that the recovery of datatype preferences doesn't overwrite a valid
// case where only bookmarks are enabled.
TEST_F(ProfileSyncServiceStartupTest, StartDontRecoverDatatypePrefs) {
  // Explicitly set Keep Everything Synced to false and have only bookmarks
  // enabled.
  pref_service()->SetBoolean(syncer::prefs::kSyncKeepEverythingSynced, false);

  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));
  sync_service()->Initialize();

  EXPECT_FALSE(
      pref_service()->GetBoolean(syncer::prefs::kSyncKeepEverythingSynced));
}

TEST_F(ProfileSyncServiceStartupTest, ManagedStartup) {
  // Service should not be started by Initialize() since it's managed.
  SimulateTestUserSignin();
  CreateSyncService(ProfileSyncService::MANUAL_START);

  // Disable sync through policy.
  pref_service()->SetBoolean(syncer::prefs::kSyncManaged, true);
  EXPECT_CALL(*component_factory(), CreateSyncEngine(_, _, _, _)).Times(0);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .Times(0);

  sync_service()->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  ON_CALL(*data_type_manager, state())
      .WillByDefault(Return(DataTypeManager::CONFIGURED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->IsEngineInitialized());
  EXPECT_TRUE(sync_service()->IsSyncActive());

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state())
      .WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop(syncer::DISABLE_SYNC));
  pref_service()->SetBoolean(syncer::prefs::kSyncManaged, true);
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  // Note that PSS no longer references |data_type_manager| after stopping.

  // When switching back to unmanaged, the state should change but sync should
  // not start automatically because IsFirstSetupComplete() will be false.
  // A new DataTypeManager should not be created.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*component_factory(), CreateDataTypeManager(_, _, _, _, _, _))
      .Times(0);
  pref_service()->ClearPref(syncer::prefs::kSyncManaged);
  EXPECT_FALSE(sync_service()->IsEngineInitialized());
  EXPECT_FALSE(sync_service()->IsSyncActive());
}

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  sync_service()->SetFirstSetupComplete();
  SetUpFakeSyncEngine();
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManagerMock();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  DataTypeManager::ConfigureResult result(status, syncer::ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_, _))
      .WillRepeatedly(
          DoAll(InvokeOnConfigureStart(sync_service()),
                InvokeOnConfigureDone(sync_service(),
                                      base::BindRepeating(&SetError), result)));
  EXPECT_CALL(*data_type_manager, state())
      .WillOnce(Return(DataTypeManager::STOPPED));
  ON_CALL(*data_type_manager, IsNigoriEnabled()).WillByDefault(Return(true));
  sync_service()->Initialize();
  EXPECT_TRUE(sync_service()->HasUnrecoverableError());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  CreateSyncService(ProfileSyncService::MANUAL_START);
  SimulateTestUserSignin();
  FakeSyncEngine* fake_engine = SetUpFakeSyncEngine();
  fake_engine->set_fail_initial_download(true);

  pref_service()->ClearPref(syncer::prefs::kSyncFirstSetupComplete);

  sync_service()->Initialize();

  auto sync_blocker = sync_service()->GetSetupInProgressHandle();
  sync_blocker.reset();
  EXPECT_FALSE(sync_service()->IsSyncActive());
}

}  // namespace browser_sync
