// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_data_type_controller.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::LocalDeviceInfoProviderMock;

namespace sync_sessions {

namespace {

const char* kSavingBrowserHistoryDisabled = "history_disabled";

class SessionDataTypeControllerTest : public testing::Test,
                                      public syncer::FakeSyncClient {
 public:
  SessionDataTypeControllerTest()
      : syncer::FakeSyncClient(&profile_sync_factory_),
        load_finished_(false),
        last_type_(syncer::UNSPECIFIED) {}
  ~SessionDataTypeControllerTest() override {}

  // FakeSyncClient overrides.
  PrefService* GetPrefService() override { return &prefs_; }

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(kSavingBrowserHistoryDisabled,
                                           false);

    local_device_ = std::make_unique<LocalDeviceInfoProviderMock>(
        "cache_guid", "Wayne Gretzky's Hacking Box", "Chromium 10k",
        "Chrome 10k", sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id");

    controller_ = std::make_unique<SessionDataTypeController>(
        base::DoNothing(), this, local_device_.get(),
        kSavingBrowserHistoryDisabled);

    load_finished_ = false;
    last_type_ = syncer::UNSPECIFIED;
    last_error_ = syncer::SyncError();
  }

  void Start() {
    controller_->LoadModels(
        base::Bind(&SessionDataTypeControllerTest::OnLoadFinished,
                   base::Unretained(this)));
  }

  void OnLoadFinished(syncer::ModelType type, const syncer::SyncError& error) {
    load_finished_ = true;
    last_type_ = type;
    last_error_ = error;
  }

  testing::AssertionResult LoadResult() {
    if (!load_finished_) {
      return testing::AssertionFailure() << "OnLoadFinished wasn't called";
    }

    if (last_error_.IsSet()) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a SyncError: "
             << last_error_.ToString();
    }

    if (last_type_ != syncer::SESSIONS) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a wrong sync type: "
             << last_type_;
    }

    return testing::AssertionSuccess();
  }

  bool load_finished() const { return load_finished_; }
  LocalDeviceInfoProviderMock* local_device() { return local_device_.get(); }
  SessionDataTypeController* controller() { return controller_.get(); }

 private:
  base::MessageLoop message_loop_;
  TestingPrefServiceSimple prefs_;
  syncer::SyncApiComponentFactoryMock profile_sync_factory_;
  std::unique_ptr<LocalDeviceInfoProviderMock> local_device_;
  std::unique_ptr<SessionDataTypeController> controller_;

  bool load_finished_;
  syncer::ModelType last_type_;
  syncer::SyncError last_error_;
};

TEST_F(SessionDataTypeControllerTest, StartModels) {
  Start();
  EXPECT_EQ(syncer::DataTypeController::MODEL_LOADED, controller()->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(SessionDataTypeControllerTest, StartModelsDelayedByLocalDevice) {
  local_device()->SetInitialized(false);
  Start();
  EXPECT_FALSE(load_finished());
  EXPECT_EQ(syncer::DataTypeController::MODEL_STARTING, controller()->state());

  local_device()->SetInitialized(true);
  EXPECT_EQ(syncer::DataTypeController::MODEL_LOADED, controller()->state());
  EXPECT_TRUE(LoadResult());
}

}  // namespace

}  // namespace sync_sessions
