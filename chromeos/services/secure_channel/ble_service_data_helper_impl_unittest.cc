// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_service_data_helper_impl.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "components/cryptauth/ble/ble_advertisement_generator.h"
#include "components/cryptauth/ble/fake_ble_advertisement_generator.h"
#include "components/cryptauth/fake_background_eid_generator.h"
#include "components/cryptauth/mock_foreground_eid_generator.h"
#include "components/cryptauth/remote_device_cache.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const size_t kNumTestDevices = 3;

const size_t kNumBytesInBackgroundAdvertisementServiceData = 2;
const size_t kMinNumBytesInForegroundAdvertisementServiceData = 4;

const char current_eid_data[] = "currentEidData";
const int64_t current_eid_start_ms = 1000L;
const int64_t current_eid_end_ms = 2000L;

const char adjacent_eid_data[] = "adjacentEidData";
const int64_t adjacent_eid_start_ms = 2000L;
const int64_t adjacent_eid_end_ms = 3000L;

const char fake_beacon_seed1_data[] = "fakeBeaconSeed1Data";
const int64_t fake_beacon_seed1_start_ms = current_eid_start_ms;
const int64_t fake_beacon_seed1_end_ms = current_eid_end_ms;

const char fake_beacon_seed2_data[] = "fakeBeaconSeed2Data";
const int64_t fake_beacon_seed2_start_ms = adjacent_eid_start_ms;
const int64_t fake_beacon_seed2_end_ms = adjacent_eid_end_ms;

std::unique_ptr<cryptauth::ForegroundEidGenerator::EidData>
CreateFakeBackgroundScanFilter() {
  cryptauth::DataWithTimestamp current(current_eid_data, current_eid_start_ms,
                                       current_eid_end_ms);

  std::unique_ptr<cryptauth::DataWithTimestamp> adjacent =
      std::make_unique<cryptauth::DataWithTimestamp>(
          adjacent_eid_data, adjacent_eid_start_ms, adjacent_eid_end_ms);

  return std::make_unique<cryptauth::ForegroundEidGenerator::EidData>(
      current, std::move(adjacent));
}

std::vector<cryptauth::BeaconSeed> CreateFakeBeaconSeeds(int id) {
  std::string id_str = std::to_string(id);

  cryptauth::BeaconSeed seed1;
  seed1.set_data(fake_beacon_seed1_data + id_str);
  seed1.set_start_time_millis(fake_beacon_seed1_start_ms * id);
  seed1.set_start_time_millis(fake_beacon_seed1_end_ms * id);

  cryptauth::BeaconSeed seed2;
  seed2.set_data(fake_beacon_seed2_data + id_str);
  seed2.set_start_time_millis(fake_beacon_seed2_start_ms * id);
  seed2.set_start_time_millis(fake_beacon_seed2_end_ms * id);

  std::vector<cryptauth::BeaconSeed> seeds = {seed1, seed2};
  return seeds;
}

cryptauth::RemoteDeviceRef CreateLocalDevice(int id) {
  return cryptauth::RemoteDeviceRefBuilder()
      .SetPublicKey("local public key " + std::to_string(id))
      .SetBeaconSeeds(CreateFakeBeaconSeeds(id))
      .Build();
}

}  // namespace

class SecureChannelBleServiceDataHelperImplTest : public testing::Test {
 protected:
  SecureChannelBleServiceDataHelperImplTest()
      : test_local_device_1_(CreateLocalDevice(1)),
        test_local_device_2_(CreateLocalDevice(2)),
        test_remote_devices_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)),
        fake_advertisement_(
            cryptauth::DataWithTimestamp("advertisement1", 1000L, 2000L)) {
    device_id_pair_set_.emplace(test_remote_devices_[0].GetDeviceId(),
                                test_local_device_1_.GetDeviceId());
    device_id_pair_set_.emplace(test_remote_devices_[1].GetDeviceId(),
                                test_local_device_1_.GetDeviceId());
    device_id_pair_set_.emplace(test_remote_devices_[2].GetDeviceId(),
                                test_local_device_2_.GetDeviceId());
  }

  ~SecureChannelBleServiceDataHelperImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_ble_advertisement_generator_ =
        std::make_unique<cryptauth::FakeBleAdvertisementGenerator>();
    cryptauth::BleAdvertisementGenerator::SetInstanceForTesting(
        fake_ble_advertisement_generator_.get());
    fake_ble_advertisement_generator_->set_advertisement(
        std::make_unique<cryptauth::DataWithTimestamp>(fake_advertisement_));

    auto mock_foreground_eid_generator =
        std::make_unique<cryptauth::MockForegroundEidGenerator>();
    mock_foreground_eid_generator_ = mock_foreground_eid_generator.get();
    mock_foreground_eid_generator_->set_background_scan_filter(
        CreateFakeBackgroundScanFilter());

    auto fake_background_eid_generator =
        std::make_unique<cryptauth::FakeBackgroundEidGenerator>();
    fake_background_eid_generator_ = fake_background_eid_generator.get();

    remote_device_cache_ =
        cryptauth::RemoteDeviceCache::Factory::Get()->BuildInstance();

    cryptauth::RemoteDeviceList devices;
    devices.push_back(*cryptauth::GetMutableRemoteDevice(test_local_device_1_));
    devices.push_back(*cryptauth::GetMutableRemoteDevice(test_local_device_2_));
    std::transform(
        test_remote_devices_.begin(), test_remote_devices_.end(),
        std::back_inserter(devices), [](auto remote_device_ref) {
          return *cryptauth::GetMutableRemoteDevice(remote_device_ref);
        });
    remote_device_cache_->SetRemoteDevices(devices);

    helper_ = BleServiceDataHelperImpl::Factory::Get()->BuildInstance(
        remote_device_cache_.get());

    static_cast<BleServiceDataHelperImpl*>(helper_.get())
        ->SetTestDoubles(std::move(fake_background_eid_generator),
                         std::move(mock_foreground_eid_generator));
  }

  void TearDown() override {
    cryptauth::BleAdvertisementGenerator::SetInstanceForTesting(nullptr);
  }

  std::unique_ptr<cryptauth::FakeBleAdvertisementGenerator>
      fake_ble_advertisement_generator_;
  cryptauth::MockForegroundEidGenerator* mock_foreground_eid_generator_;
  cryptauth::FakeBackgroundEidGenerator* fake_background_eid_generator_;

  std::unique_ptr<cryptauth::RemoteDeviceCache> remote_device_cache_;

  std::unique_ptr<BleServiceDataHelper> helper_;

  cryptauth::RemoteDeviceRef test_local_device_1_;
  cryptauth::RemoteDeviceRef test_local_device_2_;
  cryptauth::RemoteDeviceRefList test_remote_devices_;
  secure_channel::DeviceIdPairSet device_id_pair_set_;

  cryptauth::DataWithTimestamp fake_advertisement_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecureChannelBleServiceDataHelperImplTest);
};

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_CannotGenerateAdvertisement) {
  fake_ble_advertisement_generator_->set_advertisement(nullptr);
  EXPECT_FALSE(
      helper_->GenerateForegroundAdvertisement(secure_channel::DeviceIdPair(
          test_remote_devices_[0].GetDeviceId() /* remote_device_id */,
          test_local_device_1_.GetDeviceId() /* local_device_id */)));
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement) {
  auto data_with_timestamp =
      helper_->GenerateForegroundAdvertisement(secure_channel::DeviceIdPair(
          test_remote_devices_[0].GetDeviceId() /* remote_device_id */,
          test_local_device_1_.GetDeviceId() /* local_device_id */));
  EXPECT_EQ(fake_advertisement_, *data_with_timestamp);
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_InvalidLocalDevice) {
  EXPECT_FALSE(
      helper_->GenerateForegroundAdvertisement(secure_channel::DeviceIdPair(
          test_remote_devices_[0].GetDeviceId() /* remote_device_id */,
          "invalid local device id" /* local_device_id */)));
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestGenerateForegroundAdvertisement_InvalidRemoteDevice) {
  EXPECT_FALSE(
      helper_->GenerateForegroundAdvertisement(secure_channel::DeviceIdPair(
          "invalid remote device id" /* remote_device_id */,
          test_local_device_1_.GetDeviceId() /* local_device_id */)));
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_InvalidAdvertisementLength) {
  std::string invalid_service_data = "a";
  mock_foreground_eid_generator_->set_identified_device_id(
      test_remote_devices_[0].GetDeviceId());

  auto device_with_background_bool =
      helper_->IdentifyRemoteDevice(invalid_service_data, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_FALSE(device_with_background_bool);
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement) {
  std::string valid_service_data_for_registered_device = "abcde";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kMinNumBytesInForegroundAdvertisementServiceData);

  mock_foreground_eid_generator_->set_identified_device_id(
      test_remote_devices_[0].GetDeviceId());

  auto device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(1, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_FALSE(device_with_background_bool->second);

  // Ensure that other local device IDs in device_id_pair_set_ are considered.
  mock_foreground_eid_generator_->set_identified_device_id(
      test_remote_devices_[2].GetDeviceId());

  device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[2].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_FALSE(device_with_background_bool->second);
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_ForegroundAdvertisement_NoRegisteredDevice) {
  std::string valid_service_data = "abcde";
  ASSERT_TRUE(valid_service_data.size() >=
              kMinNumBytesInForegroundAdvertisementServiceData);

  auto device_with_background_bool =
      helper_->IdentifyRemoteDevice(valid_service_data, device_id_pair_set_);

  EXPECT_EQ(2, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(0, fake_background_eid_generator_->num_identify_calls());
  EXPECT_FALSE(device_with_background_bool);
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement) {
  std::string valid_service_data_for_registered_device = "ab";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kNumBytesInBackgroundAdvertisementServiceData);

  fake_background_eid_generator_->set_identified_device_id(
      test_remote_devices_[0].GetDeviceId());

  auto device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(1, fake_background_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[0].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_TRUE(device_with_background_bool->second);

  // Ensure that other local device IDs in device_id_pair_set_ are considered.
  fake_background_eid_generator_->set_identified_device_id(
      test_remote_devices_[2].GetDeviceId());

  device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(test_remote_devices_[2].GetDeviceId(),
            device_with_background_bool->first.GetDeviceId());
  EXPECT_TRUE(device_with_background_bool->second);
}

TEST_F(SecureChannelBleServiceDataHelperImplTest,
       TestIdentifyRemoteDevice_BackgroundAdvertisement_NoRegisteredDevice) {
  std::string valid_service_data_for_registered_device = "ab";
  ASSERT_TRUE(valid_service_data_for_registered_device.size() >=
              kNumBytesInBackgroundAdvertisementServiceData);

  auto device_with_background_bool = helper_->IdentifyRemoteDevice(
      valid_service_data_for_registered_device, device_id_pair_set_);

  EXPECT_EQ(0, mock_foreground_eid_generator_->num_identify_calls());
  EXPECT_EQ(2, fake_background_eid_generator_->num_identify_calls());
  EXPECT_FALSE(device_with_background_bool);
}

}  // namespace secure_channel

}  // namespace chromeos
