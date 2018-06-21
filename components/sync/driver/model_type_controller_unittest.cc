// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/engine/commit_queue.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/engine/fake_model_type_processor.h"
#include "components/sync/engine/model_type_configurer.h"
#include "components/sync/engine/model_type_processor_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;

const ModelType kTestModelType = AUTOFILL;
const char kCacheGuid[] = "SomeCacheGuid";
const char kAccountId[] = "SomeAccountId";

void SetBool(bool* called, bool* out, bool in) {
  *called = true;
  *out = in;
}

// A simple processor that trackes connected state.
class TestModelTypeProcessor
    : public FakeModelTypeProcessor,
      public base::SupportsWeakPtr<TestModelTypeProcessor> {
 public:
  TestModelTypeProcessor() {}

  bool is_connected() const { return is_connected_; }

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<CommitQueue> commit_queue) override {
    is_connected_ = true;
  }

  void DisconnectSync() override { is_connected_ = false; }

 private:
  bool is_connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestModelTypeProcessor);
};

// A delegate for testing that connects using a thread-jumping proxy, tracks
// connected state, and counts DisableSync calls.
class TestDelegate : public ModelTypeControllerDelegate,
                     public base::SupportsWeakPtr<TestDelegate> {
 public:
  TestDelegate() {}

  void set_initial_sync_done(bool initial_sync_done) {
    initial_sync_done_ = initial_sync_done;
  }

  bool is_processor_connected() const { return processor_.is_connected(); }

  int cleared_metadata_count() const { return cleared_metadata_count_; }

  // ModelTypeControllerDelegate overrides.
  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override {
    std::unique_ptr<DataTypeActivationResponse> activation_response =
        std::make_unique<DataTypeActivationResponse>();
    activation_response->model_type_state.set_initial_sync_done(
        initial_sync_done_);
    activation_response->type_processor =
        std::make_unique<ModelTypeProcessorProxy>(
            base::AsWeakPtr(&processor_), base::ThreadTaskRunnerHandle::Get());
    std::move(callback).Run(std::move(activation_response));
  }

  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override {
    if (metadata_fate == CLEAR_METADATA) {
      cleared_metadata_count_++;
    }
  }

  void GetAllNodesForDebugging(AllNodesCallback callback) override {}

  void GetStatusCountersForDebugging(StatusCountersCallback callback) override {
  }

  void RecordMemoryUsageHistogram() override {}

 private:
  int cleared_metadata_count_ = 0;
  bool initial_sync_done_ = false;
  TestModelTypeProcessor processor_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

// A ModelTypeConfigurer that just connects USS types.
class TestModelTypeConfigurer : public ModelTypeConfigurer {
 public:
  TestModelTypeConfigurer() {}
  ~TestModelTypeConfigurer() override {}

  void ConfigureDataTypes(ConfigureParams params) override {
    NOTREACHED() << "Not implemented.";
  }

  void RegisterDirectoryDataType(ModelType type,
                                 ModelSafeGroup group) override {
    NOTREACHED() << "Not implemented.";
  }

  void UnregisterDirectoryDataType(ModelType type) override {
    NOTREACHED() << "Not implemented.";
  }

  void ActivateDirectoryDataType(ModelType type,
                                 ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override {
    NOTREACHED() << "Not implemented.";
  }

  void DeactivateDirectoryDataType(ModelType type) override {
    NOTREACHED() << "Not implemented.";
  }

  void ActivateNonBlockingDataType(ModelType type,
                                   std::unique_ptr<DataTypeActivationResponse>
                                       activation_response) override {
    DCHECK_EQ(kTestModelType, type);
    DCHECK(!processor_);
    processor_ = std::move(activation_response->type_processor);
    processor_->ConnectSync(nullptr);
  }

  void DeactivateNonBlockingDataType(ModelType type) override {
    DCHECK_EQ(kTestModelType, type);
    DCHECK(processor_);
    processor_->DisconnectSync();
    processor_.reset();
  }

 private:
  std::unique_ptr<ModelTypeProcessor> processor_;
};

class TestSyncService : public FakeSyncService {
 public:
  TestSyncService() {
    local_device_info_provider_.Initialize(kCacheGuid,
                                           /*signin_scoped_device_id=*/"");
  }

  ~TestSyncService() override {}

  const LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() const override {
    return &local_device_info_provider_;
  }

 private:
  LocalDeviceInfoProviderMock local_device_info_provider_;
};

}  // namespace

class ModelTypeControllerTest : public testing::Test {
 public:
  ModelTypeControllerTest() : model_thread_("modelthread") {
    model_thread_.Start();

    AccountInfo account_info;
    account_info.account_id = kAccountId;
    sync_service_.SetAuthenticatedAccountInfo(account_info);

    ON_CALL(sync_client_mock_, GetSyncService())
        .WillByDefault(testing::Return(&sync_service_));
    ON_CALL(sync_client_mock_, GetControllerDelegateForModelType(_))
        .WillByDefault(testing::Return(base::AsWeakPtr(&delegate_)));

    controller_ = std::make_unique<ModelTypeController>(
        kTestModelType, &sync_client_mock_, model_thread_.task_runner());
  }

  ~ModelTypeControllerTest() {
    PumpModelThread();
    PumpUIThread();
  }

  void LoadModels() {
    controller_->LoadModels(base::Bind(&ModelTypeControllerTest::LoadModelsDone,
                                       base::Unretained(this)));
  }

  void RegisterWithBackend(bool expect_downloaded) {
    bool called = false;
    bool downloaded;
    controller_->RegisterWithBackend(base::Bind(&SetBool, &called, &downloaded),
                                     &configurer_);
    EXPECT_TRUE(called);
    EXPECT_EQ(expect_downloaded, downloaded);
  }

  void StartAssociating() {
    controller_->StartAssociating(base::Bind(
        &ModelTypeControllerTest::AssociationDone, base::Unretained(this)));
    // The callback is expected to be promptly called.
    EXPECT_TRUE(association_callback_called_);
  }

  void DeactivateDataTypeAndStop(SyncStopMetadataFate metadata_fate) {
    controller_->DeactivateDataType(&configurer_);
    controller_->Stop(metadata_fate);
  }

  // These threads can ping-pong for a bit so we run the model thread twice.
  void RunAllTasks() {
    PumpModelThread();
    PumpUIThread();
    PumpModelThread();
  }

  // Runs any tasks posted on the model thread.
  void PumpModelThread() {
    base::RunLoop run_loop;
    model_thread_.task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

  void ExpectProcessorConnected(bool is_connected) {
    if (model_thread_.task_runner()->BelongsToCurrentThread()) {
      EXPECT_EQ(is_connected, delegate_.is_processor_connected());
    } else {
      model_thread_.task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&ModelTypeControllerTest::ExpectProcessorConnected,
                     base::Unretained(this), is_connected));
      PumpModelThread();
    }
  }

  void SetInitialSyncDone(bool initial_sync_done) {
    delegate_.set_initial_sync_done(initial_sync_done);
  }

  DataTypeController* controller() { return controller_.get(); }
  int load_models_done_count() { return load_models_done_count_; }
  int cleared_metadata_count() { return delegate_.cleared_metadata_count(); }
  SyncError load_models_last_error() { return load_models_last_error_; }

 private:
  // Runs any tasks posted on the UI thread.
  void PumpUIThread() { base::RunLoop().RunUntilIdle(); }

  void LoadModelsDone(ModelType type, const SyncError& error) {
    load_models_done_count_++;
    load_models_last_error_ = error;
  }

  void AssociationDone(DataTypeController::ConfigureResult result,
                       const SyncMergeResult& local_merge_result,
                       const SyncMergeResult& syncer_merge_result) {
    EXPECT_FALSE(association_callback_called_);
    EXPECT_EQ(DataTypeController::OK, result);
    association_callback_called_ = true;
  }

  int load_models_done_count_ = 0;
  bool association_callback_called_ = false;
  SyncError load_models_last_error_;

  base::MessageLoop message_loop_;
  base::Thread model_thread_;
  TestDelegate delegate_;
  TestSyncService sync_service_;
  testing::NiceMock<SyncClientMock> sync_client_mock_;
  TestModelTypeConfigurer configurer_;
  std::unique_ptr<ModelTypeController> controller_;
};

TEST_F(ModelTypeControllerTest, InitialState) {
  EXPECT_EQ(kTestModelType, controller()->type());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

TEST_F(ModelTypeControllerTest, LoadModelsOnBackendThread) {
  LoadModels();
  EXPECT_EQ(DataTypeController::MODEL_STARTING, controller()->state());
  RunAllTasks();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
  EXPECT_EQ(1, load_models_done_count());
  EXPECT_FALSE(load_models_last_error().IsSet());
  ExpectProcessorConnected(false);
}

TEST_F(ModelTypeControllerTest, LoadModelsTwice) {
  LoadModels();
  RunAllTasks();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
  EXPECT_FALSE(load_models_last_error().IsSet());
  // A second LoadModels call should set the error.
  LoadModels();
  EXPECT_TRUE(load_models_last_error().IsSet());
}

TEST_F(ModelTypeControllerTest, Activate) {
  LoadModels();
  RunAllTasks();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
  RegisterWithBackend(false);
  ExpectProcessorConnected(true);

  StartAssociating();
  EXPECT_EQ(DataTypeController::RUNNING, controller()->state());
}

TEST_F(ModelTypeControllerTest, ActivateWithInitialSyncDone) {
  SetInitialSyncDone(true);
  LoadModels();
  RunAllTasks();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller()->state());
  RegisterWithBackend(true);
  ExpectProcessorConnected(true);
}

TEST_F(ModelTypeControllerTest, Stop) {
  LoadModels();
  RunAllTasks();
  RegisterWithBackend(false);
  ExpectProcessorConnected(true);

  StartAssociating();

  DeactivateDataTypeAndStop(KEEP_METADATA);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
}

// Test emulates normal browser shutdown. Ensures that metadata was not cleared.
TEST_F(ModelTypeControllerTest, StopWhenDatatypeEnabled) {
  LoadModels();
  RunAllTasks();
  StartAssociating();

  DeactivateDataTypeAndStop(KEEP_METADATA);
  RunAllTasks();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
  // Ensures that metadata was not cleared.
  EXPECT_EQ(0, cleared_metadata_count());
  ExpectProcessorConnected(false);
}

// Test emulates scenario when user disables datatype. Metadata should be
// cleared.
TEST_F(ModelTypeControllerTest, StopWhenDatatypeDisabled) {
  LoadModels();
  RunAllTasks();
  StartAssociating();

  DeactivateDataTypeAndStop(CLEAR_METADATA);
  RunAllTasks();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
  // Ensures that metadata was cleared.
  EXPECT_EQ(1, cleared_metadata_count());
  ExpectProcessorConnected(false);
}

// Test emulates disabling sync when datatype is not loaded yet. Metadata should
// not be cleared as the delegate is potentially not ready to handle it.
TEST_F(ModelTypeControllerTest, StopBeforeLoadModels) {
  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());

  controller()->Stop(CLEAR_METADATA);

  EXPECT_EQ(DataTypeController::NOT_RUNNING, controller()->state());
  // Ensure that DisableSync is not called.
  EXPECT_EQ(0, cleared_metadata_count());
}

}  // namespace syncer
