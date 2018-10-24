// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_service_data_helper.h"

#include "base/logging.h"
#include "chromeos/components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace secure_channel {

BleServiceDataHelper::BleServiceDataHelper() = default;

BleServiceDataHelper::~BleServiceDataHelper() = default;

base::Optional<BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelper::IdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  base::Optional<DeviceWithBackgroundBool>
      potential_device_with_background_bool =
          PerformIdentifyRemoteDevice(service_data, device_id_pair_set);

  if (!potential_device_with_background_bool)
    return base::nullopt;

  const std::string remote_device_id =
      potential_device_with_background_bool->first.GetDeviceId();
  for (const auto& device_id_pair : device_id_pair_set) {
    if (remote_device_id == device_id_pair.remote_device_id())
      return potential_device_with_background_bool;
  }

  PA_LOG(ERROR) << "BleServiceDataHelper::IdentifyRemoteDevice(): Identified "
                   "device was not present in the provided DeviceIdPairSet.";
  NOTREACHED();
  return base::nullopt;
}

}  // namespace secure_channel

}  // namespace chromeos
