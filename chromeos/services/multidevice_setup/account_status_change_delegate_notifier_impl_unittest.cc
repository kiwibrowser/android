// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier_impl.h"

#include <string>
#include <vector>

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/fake_setup_flow_completion_recorder.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const int64_t kTestTimeMillis = 1500000000000;
constexpr const char* const kDummyKeys[] = {"publicKey0", "publicKey1",
                                            "publicKey2"};
const char kLocalDeviceKey[] = "publicKeyOfLocalDevice";

cryptauth::RemoteDeviceRef BuildHostCandidate(
    std::string public_key,
    cryptauth::SoftwareFeatureState better_together_host_state) {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetPublicKey(public_key)
      .SetSoftwareFeatureState(cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                               better_together_host_state)
      .Build();
}

cryptauth::RemoteDeviceRef BuildLocalDevice() {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetPublicKey(kLocalDeviceKey)
      .SetSoftwareFeatureState(
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
          cryptauth::SoftwareFeatureState::kSupported)
      .Build();
}

}  // namespace

class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest
    : public testing::Test {
 protected:
  MultiDeviceSetupAccountStatusChangeDelegateNotifierTest()
      : local_device_(BuildLocalDevice()) {}

  ~MultiDeviceSetupAccountStatusChangeDelegateNotifierTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    AccountStatusChangeDelegateNotifierImpl::RegisterPrefs(
        test_pref_service_->registry());

    fake_delegate_ = std::make_unique<FakeAccountStatusChangeDelegate>();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    fake_setup_flow_completion_recorder_ =
        std::make_unique<FakeSetupFlowCompletionRecorder>();

    fake_device_sync_client_->set_local_device_metadata(local_device_);
    test_clock_->SetNow(base::Time::FromJavaTime(kTestTimeMillis));
  }

  void BuildAccountStatusChangeDelegateNotifier() {
    delegate_notifier_ =
        AccountStatusChangeDelegateNotifierImpl::Factory::Get()->BuildInstance(
            fake_device_sync_client_.get(), test_pref_service_.get(),
            fake_setup_flow_completion_recorder_.get(), test_clock_.get());
  }

  void SetNewUserPotentialHostExistsTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(AccountStatusChangeDelegateNotifierImpl::
                                     kNewUserPotentialHostExistsPrefName,
                                 timestamp);
  }

  void SetExistingUserChromebookAddedTimestamp(int64_t timestamp) {
    test_pref_service_->SetInt64(AccountStatusChangeDelegateNotifierImpl::
                                     kExistingUserChromebookAddedPrefName,
                                 timestamp);
  }

  void SetHostFromPreviousSession(std::string old_host_key) {
    test_pref_service_->SetString(AccountStatusChangeDelegateNotifierImpl::
                                      kHostPublicKeyFromMostRecentSyncPrefName,
                                  old_host_key);
  }

  void SetAccountStatusChangeDelegatePtr() {
    delegate_notifier_->SetAccountStatusChangeDelegatePtr(
        fake_delegate_->GenerateInterfacePtr());
    delegate_notifier_->FlushForTesting();
  }

  void RecordSetupFlowCompletionAtEarlierTime(const base::Time& earlier_time) {
    fake_setup_flow_completion_recorder_->set_current_time(earlier_time);

    SetupFlowCompletionRecorder* derived_ptr =
        static_cast<SetupFlowCompletionRecorder*>(
            fake_setup_flow_completion_recorder_.get());
    derived_ptr->RecordCompletion();
  }

  void NotifyNewDevicesSynced() {
    fake_device_sync_client_->NotifyNewDevicesSynced();
    delegate_notifier_->FlushForTesting();
  }

  // Represents setting preexisting devices on account and therefore does not
  // trigger a notification.
  void SetNonLocalSyncedDevices(
      cryptauth::RemoteDeviceRefList* device_ref_list) {
    // The local device should always be on the device list.
    device_ref_list->push_back(local_device_);
    fake_device_sync_client_->set_synced_devices(*device_ref_list);
  }

  void UpdateNonLocalSyncedDevices(
      cryptauth::RemoteDeviceRefList* device_ref_list) {
    SetNonLocalSyncedDevices(device_ref_list);
    NotifyNewDevicesSynced();
  }

  // Setup to simulate a the situation when the local device is ready to be
  // Better Together client enabled (i.e. enabling and syncing would trigger a
  // Chromebook added event) with a single enabled host.
  void PrepareToEnableLocalDeviceAsClient() {
    cryptauth::RemoteDeviceRefList device_ref_list =
        cryptauth::RemoteDeviceRefList{BuildHostCandidate(
            kDummyKeys[0], cryptauth::SoftwareFeatureState::kEnabled)};
    SetNonLocalSyncedDevices(&device_ref_list);
    BuildAccountStatusChangeDelegateNotifier();
  }

  // If the account as an enabled host, the local device will enable its
  // MultiDevice client feature in the background.
  void EnableLocalDeviceAsClient() {
    cryptauth::GetMutableRemoteDevice(local_device_)
        ->software_features
            [cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT] =
        cryptauth::SoftwareFeatureState::kEnabled;
  }

  int64_t GetNewUserPotentialHostExistsTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kNewUserPotentialHostExistsPrefName);
  }

  int64_t GetExistingUserChromebookAddedTimestamp() {
    return test_pref_service_->GetInt64(
        AccountStatusChangeDelegateNotifierImpl::
            kExistingUserChromebookAddedPrefName);
  }

  FakeAccountStatusChangeDelegate* fake_delegate() {
    return fake_delegate_.get();
  }

 private:
  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  cryptauth::RemoteDeviceRef local_device_;

  std::unique_ptr<FakeAccountStatusChangeDelegate> fake_delegate_;

  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<FakeSetupFlowCompletionRecorder>
      fake_setup_flow_completion_recorder_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<AccountStatusChangeDelegateNotifier> delegate_notifier_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiDeviceSetupAccountStatusChangeDelegateNotifierTest);
};

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SetObserverWithSupportedHost) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       SupportedHostAddedLater) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{BuildHostCandidate(
          kDummyKeys[0], cryptauth::SoftwareFeatureState::kNotSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(0u, GetNewUserPotentialHostExistsTimestamp());

  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kNotSupported),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kSupported)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());
  EXPECT_EQ(kTestTimeMillis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ExistingEnabledHostPreventsNewUserEvent) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported),
          BuildHostCandidate(kDummyKeys[2],
                             cryptauth::SoftwareFeatureState::kEnabled)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoNewUserEventWithoutObserverSet) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();
  // All conditions for new user event are now satisfied except for setting a
  // delegate.

  // No delegate is set before the sync.
  NotifyNewDevicesSynced();
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OldTimestampInPreferencesPreventsNewUserFlow) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kNotSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();
  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetNewUserPotentialHostExistsTimestamp(earlier_test_time_millis);

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(0u, fake_delegate()->num_new_user_events_handled());
  // Timestamp was not overwritten by clock.
  EXPECT_EQ(earlier_test_time_millis, GetNewUserPotentialHostExistsTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       MultipleSyncsOnlyTriggerOneNewUserEvent) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{BuildHostCandidate(
          kDummyKeys[0], cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());

  NotifyNewDevicesSynced();
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());

  NotifyNewDevicesSynced();
  EXPECT_EQ(1u, fake_delegate()->num_new_user_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NotifiesObserverForHostSwitchEvents) {
  const std::string initial_host_key = kDummyKeys[0];
  const std::string alternative_host_key_1 = kDummyKeys[1];
  const std::string alternative_host_key_2 = kDummyKeys[2];
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(initial_host_key,
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(alternative_host_key_1,
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();
  EnableLocalDeviceAsClient();

  SetAccountStatusChangeDelegatePtr();
  // Verify the delegate initializes to 0.
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Switch hosts.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(initial_host_key,
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(alternative_host_key_1,
                         cryptauth::SoftwareFeatureState::kEnabled)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Adding and enabling a new host device (counts as a switch).
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(initial_host_key,
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(alternative_host_key_1,
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(alternative_host_key_2,
                         cryptauth::SoftwareFeatureState::kEnabled)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(2u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  // Removing new device and switching back to initial host.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(initial_host_key,
                         cryptauth::SoftwareFeatureState::kEnabled),
      BuildHostCandidate(alternative_host_key_1,
                         cryptauth::SoftwareFeatureState::kSupported)};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(3u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       HostSwitchedBetweenSessions) {
  const std::string old_host_key = kDummyKeys[0];
  const std::string new_host_key = kDummyKeys[1];
  SetHostFromPreviousSession(old_host_key);
  // Host switched between sessions.
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{BuildHostCandidate(
          new_host_key, cryptauth::SoftwareFeatureState::kEnabled)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();
  EnableLocalDeviceAsClient();

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(1u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutLocalDeviceClientEnabled) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();

  // Switch hosts.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kEnabled)};
  // All conditions for host switched event are now satisfied except for the
  // local device's Better Together client feature enabled.

  // No delegate is set before the update (which includes a sync).
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutExistingHost) {
  // No enabled host initially.
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kSupported),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();

  // Enable one of the host devices.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kEnabled),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kSupported)};
  EnableLocalDeviceAsClient();
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoHostSwitchedEventWithoutObserverSet) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported)};
  SetNonLocalSyncedDevices(&device_ref_list);
  BuildAccountStatusChangeDelegateNotifier();
  EnableLocalDeviceAsClient();

  // Switch hosts.
  device_ref_list = cryptauth::RemoteDeviceRefList{
      BuildHostCandidate(kDummyKeys[0],
                         cryptauth::SoftwareFeatureState::kSupported),
      BuildHostCandidate(kDummyKeys[1],
                         cryptauth::SoftwareFeatureState::kEnabled)};
  // All conditions for host switched event are now satisfied except for setting
  // an delegate.

  // No delegate is set before the update (which includes a sync).
  UpdateNonLocalSyncedDevices(&device_ref_list);
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NotifiesObserverForChromebookAddedEvents) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();

  // Verify the delegate initializes to 0.
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());

  // Another sync should not trigger an additional chromebook added event.
  NotifyNewDevicesSynced();
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       OldTimestampInPreferencesDoesNotPreventChromebookAddedEvent) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();

  int64_t earlier_test_time_millis = kTestTimeMillis / 2;
  SetExistingUserChromebookAddedTimestamp(earlier_test_time_millis);

  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(
      1u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
  // Timestamp was overwritten by clock.
  EXPECT_EQ(kTestTimeMillis, GetExistingUserChromebookAddedTimestamp());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       ChromebookAddedEventRequiresLocalDeviceToEnableClientFeature) {
  PrepareToEnableLocalDeviceAsClient();

  // Triggers event check. Note that if the local device enabled the client
  // feature, this would trigger a Chromebook added event.
  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoChromebookAddedEventIfDeviceWasUsedForUnifiedSetupFlow) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();

  base::Time completion_time = base::Time::FromJavaTime(kTestTimeMillis / 2);
  RecordSetupFlowCompletionAtEarlierTime(completion_time);

  // Triggers event check. Note that if the setup flow completion had not been
  // recorded, this would trigger a Chromebook added event.
  SetAccountStatusChangeDelegatePtr();
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       NoChromebookAddedEventWithoutObserverSet) {
  PrepareToEnableLocalDeviceAsClient();
  EnableLocalDeviceAsClient();
  // All conditions for chromebook added event are now satisfied except for
  // setting a delegate.

  // No delegate is set before the sync.
  NotifyNewDevicesSynced();
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
}

TEST_F(MultiDeviceSetupAccountStatusChangeDelegateNotifierTest,
       DeviceSyncDoesNotNotifyObserverOfDisconnectedHostsAndClients) {
  cryptauth::RemoteDeviceRefList device_ref_list =
      cryptauth::RemoteDeviceRefList{
          BuildHostCandidate(kDummyKeys[0],
                             cryptauth::SoftwareFeatureState::kEnabled),
          BuildHostCandidate(kDummyKeys[1],
                             cryptauth::SoftwareFeatureState::kSupported),
          cryptauth::RemoteDeviceRefBuilder()
              .SetPublicKey(kDummyKeys[2])
              .SetSoftwareFeatureState(
                  cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT,
                  cryptauth::SoftwareFeatureState::kEnabled)
              .Build()};
  SetNonLocalSyncedDevices(&device_ref_list);
  EnableLocalDeviceAsClient();
  BuildAccountStatusChangeDelegateNotifier();

  SetAccountStatusChangeDelegatePtr();

  NotifyNewDevicesSynced();
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());

  device_ref_list = cryptauth::RemoteDeviceRefList{};
  UpdateNonLocalSyncedDevices(&device_ref_list);
  // No new events caused by removing devices.
  EXPECT_EQ(
      0u, fake_delegate()->num_existing_user_chromebook_added_events_handled());
  EXPECT_EQ(0u,
            fake_delegate()->num_existing_user_host_switched_events_handled());
}

}  // namespace multidevice_setup

}  // namespace chromeos
