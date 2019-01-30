// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/services/device_sync/device_sync_impl.h"
#include "chromeos/services/device_sync/device_sync_service.h"
#include "chromeos/services/device_sync/fake_device_sync.h"
#include "chromeos/services/device_sync/public/mojom/constants.mojom.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/fake_gcm_device_info_provider.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kTestEmail[] = "example@gmail.com";
const char kTestGcmDeviceInfoLongDeviceId[] = "longDeviceId";
const size_t kNumTestDevices = 5u;

const cryptauth::GcmDeviceInfo& GetTestGcmDeviceInfo() {
  static const base::NoDestructor<cryptauth::GcmDeviceInfo> gcm_device_info([] {
    cryptauth::GcmDeviceInfo gcm_device_info;
    gcm_device_info.set_long_device_id(kTestGcmDeviceInfoLongDeviceId);
    return gcm_device_info;
  }());

  return *gcm_device_info;
}

class FakeDeviceSyncImplFactory : public DeviceSyncImpl::Factory {
 public:
  explicit FakeDeviceSyncImplFactory(
      std::unique_ptr<FakeDeviceSync> fake_device_sync)
      : fake_device_sync_(std::move(fake_device_sync)) {}

  ~FakeDeviceSyncImplFactory() override = default;

  // DeviceSyncImpl::Factory:
  std::unique_ptr<DeviceSyncBase> BuildInstance(
      identity::IdentityManager* identity_manager,
      gcm::GCMDriver* gcm_driver,
      service_manager::Connector* connector,
      const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
      scoped_refptr<net::URLRequestContextGetter> url_request_context)
      override {
    EXPECT_TRUE(fake_device_sync_);
    return std::move(fake_device_sync_);
  }

 private:
  std::unique_ptr<FakeDeviceSync> fake_device_sync_;
};

class FakeURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  FakeURLRequestContextGetter() : null_task_runner_(new base::NullTaskRunner) {}

  net::URLRequestContext* GetURLRequestContext() override { return nullptr; }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return null_task_runner_;
  }

 private:
  ~FakeURLRequestContextGetter() override {}

  scoped_refptr<base::SingleThreadTaskRunner> null_task_runner_;
};

class TestDeviceSyncClientObserver : public DeviceSyncClient::Observer {
 public:
  ~TestDeviceSyncClientObserver() override = default;

  void set_closure_for_enrollment_finished(base::OnceClosure closure) {
    EXPECT_FALSE(closure_for_enrollment_finished_);
    closure_for_enrollment_finished_ = std::move(closure);
  }

  void set_closure_for_new_devices_synced(base::OnceClosure closure) {
    EXPECT_FALSE(closure_for_new_devices_synced_);
    closure_for_new_devices_synced_ = std::move(closure);
  }

  void OnEnrollmentFinished() override {
    ++enrollment_finished_count_;

    EXPECT_TRUE(closure_for_enrollment_finished_);
    std::move(closure_for_enrollment_finished_).Run();
  }

  void OnNewDevicesSynced() override {
    ++new_devices_synced_count_;

    EXPECT_TRUE(closure_for_new_devices_synced_);
    std::move(closure_for_new_devices_synced_).Run();
  }

  size_t enrollment_finished_count() { return enrollment_finished_count_; }
  size_t new_devices_synced_count() { return new_devices_synced_count_; }

 private:
  size_t enrollment_finished_count_ = 0u;
  size_t new_devices_synced_count_ = 0u;

  base::OnceClosure closure_for_enrollment_finished_;
  base::OnceClosure closure_for_new_devices_synced_;
};

}  // namespace

class DeviceSyncClientImplTest : public testing::Test {
 protected:
  DeviceSyncClientImplTest()
      : test_remote_device_list_(
            cryptauth::CreateRemoteDeviceListForTest(kNumTestDevices)),
        test_remote_device_ref_list_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}

  // testing::Test:
  void SetUp() override {
    fake_gcm_driver_ = std::make_unique<gcm::FakeGCMDriver>();
    fake_gcm_device_info_provider_ =
        std::make_unique<cryptauth::FakeGcmDeviceInfoProvider>(
            GetTestGcmDeviceInfo());
    fake_url_request_context_getter_ =
        base::MakeRefCounted<FakeURLRequestContextGetter>();

    identity_test_environment_ =
        std::make_unique<identity::IdentityTestEnvironment>();
    identity_test_environment_->MakePrimaryAccountAvailable(kTestEmail);

    auto fake_device_sync = std::make_unique<FakeDeviceSync>();
    fake_device_sync_ = fake_device_sync.get();
    fake_device_sync_impl_factory_ =
        std::make_unique<FakeDeviceSyncImplFactory>(
            std::move(fake_device_sync));
    DeviceSyncImpl::Factory::SetInstanceForTesting(
        fake_device_sync_impl_factory_.get());

    auto device_sync_service = std::make_unique<DeviceSyncService>(
        identity_test_environment_->identity_manager(), fake_gcm_driver_.get(),
        fake_gcm_device_info_provider_.get(),
        fake_url_request_context_getter_.get());

    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::move(device_sync_service));

    test_observer_ = std::make_unique<TestDeviceSyncClientObserver>();

    CreateClient();
  }

  void CreateClient() {
    std::unique_ptr<service_manager::Connector> connector =
        connector_factory_->CreateConnector();

    // DeviceSyncClient's constructor posts two tasks to the TaskRunner. Idle
    // the TaskRunner so that the tasks can be run via a RunLoop later on.
    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    client_ = base::WrapUnique(
        new DeviceSyncClientImpl(connector.get(), test_task_runner));
    test_task_runner->RunUntilIdle();
  }

  void InitializeClient(bool complete_enrollment_before_sync = true) {
    client_->AddObserver(test_observer_.get());

    SendPendingMojoMessages();

    if (complete_enrollment_before_sync)
      InvokeInitialGetLocalMetadataAndThenSync();
    else
      InvokeInitialSyncAndThenGetLocalMetadata();
  }

  void InvokeInitialGetLocalMetadataAndThenSync() {
    EXPECT_FALSE(client_->GetLocalDeviceMetadata());
    EXPECT_EQ(0u, test_observer_->enrollment_finished_count());

    base::RunLoop enrollment_run_loop;
    test_observer_->set_closure_for_enrollment_finished(
        enrollment_run_loop.QuitClosure());
    fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
        test_remote_device_list_[0]);
    enrollment_run_loop.Run();

    // In the case where enrollment finishes before sync, the local device
    // metadata must still be accessible.
    EXPECT_EQ(test_remote_device_list_[0].public_key,
              client_->GetLocalDeviceMetadata()->public_key());
    EXPECT_EQ(1u, test_observer_->enrollment_finished_count());

    EXPECT_EQ(1u, client_->GetSyncedDevices().size());
    EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

    base::RunLoop sync_run_loop;
    test_observer_->set_closure_for_new_devices_synced(
        sync_run_loop.QuitClosure());
    fake_device_sync_->InvokePendingGetSyncedDevicesCallback(
        test_remote_device_list_);
    sync_run_loop.Run();

    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        client_->GetSyncedDevices(), test_remote_device_list_);
    EXPECT_EQ(1u, test_observer_->new_devices_synced_count());
  }

  void InvokeInitialSyncAndThenGetLocalMetadata() {
    EXPECT_EQ(0u, client_->GetSyncedDevices().size());
    EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

    base::RunLoop sync_run_loop;
    test_observer_->set_closure_for_new_devices_synced(
        sync_run_loop.QuitClosure());
    fake_device_sync_->InvokePendingGetSyncedDevicesCallback(
        test_remote_device_list_);
    sync_run_loop.Run();

    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        client_->GetSyncedDevices(), test_remote_device_list_);
    EXPECT_EQ(1u, test_observer_->new_devices_synced_count());

    EXPECT_FALSE(client_->GetLocalDeviceMetadata());
    EXPECT_EQ(0u, test_observer_->enrollment_finished_count());

    base::RunLoop enrollment_run_loop;
    test_observer_->set_closure_for_enrollment_finished(
        enrollment_run_loop.QuitClosure());
    fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
        test_remote_device_list_[0]);
    enrollment_run_loop.Run();
    EXPECT_EQ(test_remote_device_list_[0].public_key,
              client_->GetLocalDeviceMetadata()->public_key());

    // Ensure that the rest of the synced devices are not removed from the cache
    // when updating the local device metadata.
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        client_->GetSyncedDevices(), test_remote_device_list_);
    EXPECT_EQ(1u, test_observer_->enrollment_finished_count());
  }

  void TearDown() override {
    DeviceSyncImpl::Factory::SetInstanceForTesting(nullptr);
    client_->RemoveObserver(test_observer_.get());
  }

  void CallForceEnrollmentNow(bool expected_success) {
    fake_device_sync_->set_force_enrollment_now_completed_success(
        expected_success);

    base::RunLoop run_loop;
    client_->ForceEnrollmentNow(
        base::BindOnce(&DeviceSyncClientImplTest::OnForceEnrollmentNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(expected_success, *force_enrollment_now_completed_success_);
  }

  void CallSyncNow(bool expected_success) {
    fake_device_sync_->set_force_sync_now_completed_success(expected_success);

    base::RunLoop run_loop;
    client_->ForceSyncNow(
        base::BindOnce(&DeviceSyncClientImplTest::OnForceSyncNowCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_EQ(expected_success, *force_sync_now_completed_success_);
  }

  void CallSetSoftwareFeatureState(
      const base::Optional<std::string>& expected_error_code) {
    base::RunLoop run_loop;

    client_->SetSoftwareFeatureState(
        test_remote_device_ref_list_[0].public_key(),
        cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST, true /* enabled */,
        true /* enabled */,
        base::BindOnce(
            &DeviceSyncClientImplTest::OnSetSoftwareFeatureStateCompleted,
            base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingSetSoftwareFeatureStateCallback(
        expected_error_code);
    run_loop.Run();

    EXPECT_EQ(expected_error_code, set_software_feature_state_error_code_);
  }

  void CallFindEligibleDevices(
      const base::Optional<std::string>& expected_error_code,
      cryptauth::RemoteDeviceList expected_eligible_devices,
      cryptauth::RemoteDeviceList expected_ineligible_devices) {
    base::RunLoop run_loop;

    client_->FindEligibleDevices(
        cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
        base::BindOnce(
            &DeviceSyncClientImplTest::OnFindEligibleDevicesCompleted,
            base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingFindEligibleDevicesCallback(
        expected_error_code,
        mojom::FindEligibleDevicesResponse::New(expected_eligible_devices,
                                                expected_ineligible_devices));
    run_loop.Run();

    EXPECT_EQ(expected_error_code,
              std::get<0>(find_eligible_devices_error_code_and_response_));
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        std::get<1>(find_eligible_devices_error_code_and_response_),
        expected_eligible_devices);
    VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
        std::get<2>(find_eligible_devices_error_code_and_response_),
        expected_ineligible_devices);
  }

  void CallGetDebugInfo() {
    EXPECT_FALSE(debug_info_received_);

    base::RunLoop run_loop;

    client_->GetDebugInfo(
        base::BindOnce(&DeviceSyncClientImplTest::OnGetDebugInfoCompleted,
                       base::Unretained(this), run_loop.QuitClosure()));

    SendPendingMojoMessages();

    fake_device_sync_->InvokePendingGetDebugInfoCallback(
        mojom::DebugInfo::New());
    run_loop.Run();

    EXPECT_TRUE(debug_info_received_);
  }

  // DeviceSyncClientImpl cached its devices in a RemoteDeviceCache, which
  // stores devices in an unordered_map -- retrieved devices thus need to be
  // sorted before comparison.
  void VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      cryptauth::RemoteDeviceRefList remote_device_ref_list,
      cryptauth::RemoteDeviceList remote_device_list) {
    std::vector<std::string> ref_public_keys;
    for (auto device : remote_device_ref_list)
      ref_public_keys.push_back(device.public_key());
    std::sort(ref_public_keys.begin(), ref_public_keys.end(),
              [](auto public_key_1, auto public_key_2) {
                return public_key_1 < public_key_2;
              });

    std::vector<std::string> public_keys;
    for (auto device : remote_device_list)
      public_keys.push_back(device.public_key);
    std::sort(public_keys.begin(), public_keys.end(),
              [](auto public_key_1, auto public_key_2) {
                return public_key_1 < public_key_2;
              });

    EXPECT_EQ(ref_public_keys, public_keys);
  }

  void SendPendingMojoMessages() { client_->FlushForTesting(); }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<identity::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<gcm::FakeGCMDriver> fake_gcm_driver_;
  std::unique_ptr<cryptauth::FakeGcmDeviceInfoProvider>
      fake_gcm_device_info_provider_;
  scoped_refptr<FakeURLRequestContextGetter> fake_url_request_context_getter_;
  FakeDeviceSync* fake_device_sync_;
  std::unique_ptr<FakeDeviceSyncImplFactory> fake_device_sync_impl_factory_;
  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<TestDeviceSyncClientObserver> test_observer_;

  std::unique_ptr<DeviceSyncClientImpl> client_;

  cryptauth::RemoteDeviceList test_remote_device_list_;
  const cryptauth::RemoteDeviceRefList test_remote_device_ref_list_;

  base::Optional<bool> force_enrollment_now_completed_success_;
  base::Optional<bool> force_sync_now_completed_success_;
  base::Optional<std::string> set_software_feature_state_error_code_;
  std::tuple<base::Optional<std::string>,
             cryptauth::RemoteDeviceRefList,
             cryptauth::RemoteDeviceRefList>
      find_eligible_devices_error_code_and_response_;
  bool debug_info_received_ = false;

 private:
  void OnForceEnrollmentNowCompleted(base::OnceClosure quit_closure,
                                     bool success) {
    force_enrollment_now_completed_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnForceSyncNowCompleted(base::OnceClosure quit_closure, bool success) {
    force_sync_now_completed_success_ = success;
    std::move(quit_closure).Run();
  }

  void OnSetSoftwareFeatureStateCompleted(
      base::OnceClosure callback,
      const base::Optional<std::string>& error_code) {
    set_software_feature_state_error_code_ = error_code;
    std::move(callback).Run();
  }

  void OnFindEligibleDevicesCompleted(
      base::OnceClosure callback,
      const base::Optional<std::string>& error_code,
      cryptauth::RemoteDeviceRefList eligible_devices,
      cryptauth::RemoteDeviceRefList ineligible_devices) {
    find_eligible_devices_error_code_and_response_ =
        std::make_tuple(error_code, eligible_devices, ineligible_devices);
    std::move(callback).Run();
  }

  void OnGetDebugInfoCompleted(base::OnceClosure callback,
                               mojom::DebugInfoPtr debug_info_ptr) {
    debug_info_received_ = true;
    std::move(callback).Run();
  }

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClientImplTest);
};

TEST_F(DeviceSyncClientImplTest,
       TestCompleteInitialSyncBeforeInitialEnrollment) {
  InitializeClient(false /* complete_enrollment_before_sync */);
}

TEST_F(
    DeviceSyncClientImplTest,
    TestCompleteInitialEnrollmentBeforeInitialSync_WaitForLocalDeviceMetadata) {
  client_->AddObserver(test_observer_.get());

  SendPendingMojoMessages();

  EXPECT_FALSE(client_->GetLocalDeviceMetadata());
  EXPECT_EQ(0u, test_observer_->enrollment_finished_count());

  // Simulate local device metadata not being ready. It will be ready once
  // synced devices are returned, at which point |client_| should call
  // GetLocalMetadata() again.
  fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(base::nullopt);

  EXPECT_FALSE(client_->GetLocalDeviceMetadata());
  EXPECT_EQ(0u, test_observer_->enrollment_finished_count());

  EXPECT_EQ(0u, client_->GetSyncedDevices().size());
  EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

  base::RunLoop sync_run_loop;
  test_observer_->set_closure_for_new_devices_synced(
      sync_run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetSyncedDevicesCallback(
      test_remote_device_list_);
  sync_run_loop.Run();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), test_remote_device_list_);
  EXPECT_EQ(1u, test_observer_->new_devices_synced_count());

  SendPendingMojoMessages();

  base::RunLoop get_local_metadata_run_loop;
  test_observer_->set_closure_for_enrollment_finished(
      get_local_metadata_run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
      test_remote_device_list_[0]);
  get_local_metadata_run_loop.Run();

  EXPECT_EQ(test_remote_device_list_[0].public_key,
            client_->GetLocalDeviceMetadata()->public_key());
  EXPECT_EQ(1u, test_observer_->enrollment_finished_count());
}

TEST_F(DeviceSyncClientImplTest, TestOnEnrollmentFinished) {
  EXPECT_EQ(0u, test_observer_->enrollment_finished_count());

  InitializeClient();

  EXPECT_EQ(test_remote_device_list_[0].public_key,
            client_->GetLocalDeviceMetadata()->public_key());
  EXPECT_EQ(test_remote_device_list_[0].name,
            client_->GetLocalDeviceMetadata()->name());

  fake_device_sync_->NotifyOnEnrollmentFinished();

  // The client calls and waits for DeviceSync::GetLocalDeviceMetadata() to
  // finish before notifying observers that enrollment has finished.
  EXPECT_EQ(1u, test_observer_->enrollment_finished_count());

  SendPendingMojoMessages();

  // Update the local device metadata.
  test_remote_device_list_[0].name = "new name";

  base::RunLoop run_loop;
  test_observer_->set_closure_for_enrollment_finished(run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetLocalDeviceMetadataCallback(
      test_remote_device_list_[0]);
  run_loop.Run();

  EXPECT_EQ(2u, test_observer_->enrollment_finished_count());

  EXPECT_EQ(test_remote_device_list_[0].public_key,
            client_->GetLocalDeviceMetadata()->public_key());
  EXPECT_EQ("new name", client_->GetLocalDeviceMetadata()->name());
}

TEST_F(DeviceSyncClientImplTest, TestOnNewDevicesSynced) {
  EXPECT_EQ(0u, test_observer_->new_devices_synced_count());

  InitializeClient();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), test_remote_device_list_);

  fake_device_sync_->NotifyOnNewDevicesSynced();

  // The client calls and waits for DeviceSync::GetLocalDeviceMetadata() to
  // finish before notifying observers that enrollment has finished.
  EXPECT_EQ(1u, test_observer_->new_devices_synced_count());

  SendPendingMojoMessages();

  // Change the synced device list.
  cryptauth::RemoteDeviceList new_device_list(
      {test_remote_device_list_[0], test_remote_device_list_[1]});

  base::RunLoop run_loop;
  test_observer_->set_closure_for_new_devices_synced(run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetSyncedDevicesCallback(new_device_list);
  run_loop.Run();

  EXPECT_EQ(2u, test_observer_->new_devices_synced_count());

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), new_device_list);
}

TEST_F(DeviceSyncClientImplTest, TestForceEnrollmentNow_ExpectSuccess) {
  InitializeClient();

  CallForceEnrollmentNow(true /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestForceEnrollmentNow_ExpectFailure) {
  InitializeClient();

  CallForceEnrollmentNow(false /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestSyncNow_ExpectSuccess) {
  InitializeClient();

  CallSyncNow(true /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestSyncNow_ExpectFailure) {
  InitializeClient();

  CallSyncNow(false /* expected_success */);
}

TEST_F(DeviceSyncClientImplTest, TestGetSyncedDevices_DeviceRemovedFromCache) {
  InitializeClient();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), test_remote_device_list_);

  // Remove a device from the test list, and inform |client_|.
  cryptauth::RemoteDeviceList new_list(
      {test_remote_device_list_[0], test_remote_device_list_[1],
       test_remote_device_list_[2], test_remote_device_list_[3]});
  client_->OnNewDevicesSynced();

  SendPendingMojoMessages();

  base::RunLoop run_loop;
  test_observer_->set_closure_for_new_devices_synced(run_loop.QuitClosure());
  fake_device_sync_->InvokePendingGetSyncedDevicesCallback(new_list);
  run_loop.Run();

  VerifyRemoteDeviceRefListAndRemoteDeviceListAreEqual(
      client_->GetSyncedDevices(), new_list);
}

TEST_F(DeviceSyncClientImplTest, TestSetSoftwareFeatureState) {
  InitializeClient();

  CallSetSoftwareFeatureState("error_code");
}

TEST_F(DeviceSyncClientImplTest, TestFindEligibleDevices_NoErrorCode) {
  InitializeClient();

  cryptauth::RemoteDeviceList expected_eligible_devices(
      {test_remote_device_list_[0], test_remote_device_list_[1]});
  cryptauth::RemoteDeviceList expected_ineligible_devices(
      {test_remote_device_list_[2], test_remote_device_list_[3],
       test_remote_device_list_[4]});

  CallFindEligibleDevices(base::nullopt, expected_eligible_devices,
                          expected_ineligible_devices);
}

TEST_F(DeviceSyncClientImplTest, TestFindEligibleDevices_ErrorCode) {
  InitializeClient();

  CallFindEligibleDevices("error_code", cryptauth::RemoteDeviceList(),
                          cryptauth::RemoteDeviceList());
}

TEST_F(DeviceSyncClientImplTest, TestGetDebugInfo) {
  InitializeClient();

  CallGetDebugInfo();
}

}  // namespace device_sync

}  // namespace chromeos
