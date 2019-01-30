// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_BLE_ADVERTISEMENT_GENERATOR_H_
#define COMPONENTS_CRYPTAUTH_BLE_ADVERTISEMENT_GENERATOR_H_

#include <memory>

#include "base/macros.h"
#include "components/cryptauth/foreground_eid_generator.h"

namespace chromeos {
namespace secure_channel {
class SecureChannelBleServiceDataHelperImplTest;
}  // namespace secure_channel
namespace tether {
class BleAdvertiserImplTest;
class BleServiceDataHelperImplTest;
class AdHocBleAdvertiserImplTest;
}  // namespace tether
}  // namespace chromeos

namespace cryptauth {

class RemoteDeviceRef;

// Generates advertisements for the ProximityAuth BLE advertisement scheme.
class BleAdvertisementGenerator {
 public:
  // Generates an advertisement from the current device to |remote_device|. The
  // generated advertisement should be used immediately since it is based on the
  // current timestamp.
  static std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisement(
      RemoteDeviceRef remote_device,
      const std::string& local_device_public_key);

  virtual ~BleAdvertisementGenerator();

 protected:
  BleAdvertisementGenerator();

  virtual std::unique_ptr<DataWithTimestamp> GenerateBleAdvertisementInternal(
      RemoteDeviceRef remote_device,
      const std::string& local_device_public_key);

 private:
  friend class CryptAuthBleAdvertisementGeneratorTest;
  friend class chromeos::secure_channel::
      SecureChannelBleServiceDataHelperImplTest;
  friend class chromeos::tether::BleAdvertiserImplTest;
  friend class chromeos::tether::BleServiceDataHelperImplTest;
  friend class chromeos::tether::AdHocBleAdvertiserImplTest;

  static BleAdvertisementGenerator* instance_;

  static void SetInstanceForTesting(BleAdvertisementGenerator* test_generator);

  void SetEidGeneratorForTesting(
      std::unique_ptr<ForegroundEidGenerator> test_eid_generator);

  std::unique_ptr<ForegroundEidGenerator> eid_generator_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertisementGenerator);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_BLE_ADVERTISEMENT_GENERATOR_H_
