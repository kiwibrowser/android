// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/ble/ble_advertisement_generator.h"

#include <memory>
#include <vector>

#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {

// static
BleAdvertisementGenerator* BleAdvertisementGenerator::instance_ = nullptr;

// static
std::unique_ptr<DataWithTimestamp>
BleAdvertisementGenerator::GenerateBleAdvertisement(
    RemoteDeviceRef remote_device,
    const std::string& local_device_public_key) {
  if (!instance_)
    instance_ = new BleAdvertisementGenerator();

  return instance_->GenerateBleAdvertisementInternal(remote_device,
                                                     local_device_public_key);
}

// static
void BleAdvertisementGenerator::SetInstanceForTesting(
    BleAdvertisementGenerator* test_generator) {
  instance_ = test_generator;
}

BleAdvertisementGenerator::BleAdvertisementGenerator()
    : eid_generator_(std::make_unique<cryptauth::ForegroundEidGenerator>()) {}

BleAdvertisementGenerator::~BleAdvertisementGenerator() {}

std::unique_ptr<DataWithTimestamp>
BleAdvertisementGenerator::GenerateBleAdvertisementInternal(
    RemoteDeviceRef remote_device,
    const std::string& local_device_public_key) {
  if (local_device_public_key.empty()) {
    PA_LOG(WARNING) << "Local device's public key is empty. Cannot advertise "
                    << "with an invalid key.";
    return nullptr;
  }

  if (remote_device.beacon_seeds().empty()) {
    PA_LOG(WARNING) << "No synced seeds exist for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise without seeds.";
    return nullptr;
  }

  std::unique_ptr<DataWithTimestamp> service_data =
      eid_generator_->GenerateAdvertisement(local_device_public_key,
                                            remote_device.beacon_seeds());
  if (!service_data) {
    PA_LOG(WARNING) << "Error generating advertisement for device with ID "
                    << remote_device.GetTruncatedDeviceIdForLogs() << ". "
                    << "Cannot advertise.";
    return nullptr;
  }

  return service_data;
}

void BleAdvertisementGenerator::SetEidGeneratorForTesting(
    std::unique_ptr<ForegroundEidGenerator> test_eid_generator) {
  eid_generator_ = std::move(test_eid_generator);
}

}  // namespace cryptauth
