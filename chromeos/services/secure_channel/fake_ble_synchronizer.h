// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SYNCHRONIZER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SYNCHRONIZER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_synchronizer_base.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace secure_channel {

// Test double for BleSynchronizer.
class FakeBleSynchronizer : public BleSynchronizerBase {
 public:
  FakeBleSynchronizer();
  ~FakeBleSynchronizer() override;

  size_t GetNumCommands();

  device::BluetoothAdvertisement::Data& GetAdvertisementData(size_t index);
  const device::BluetoothAdapter::CreateAdvertisementCallback&
  GetRegisterCallback(size_t index);
  const device::BluetoothAdapter::AdvertisementErrorCallback&
  GetRegisterErrorCallback(size_t index);

  const device::BluetoothAdvertisement::SuccessCallback& GetUnregisterCallback(
      size_t index);
  const device::BluetoothAdvertisement::ErrorCallback&
  GetUnregisterErrorCallback(size_t index);

  const device::BluetoothAdapter::DiscoverySessionCallback&
  GetStartDiscoveryCallback(size_t index);
  const device::BluetoothAdapter::ErrorCallback& GetStartDiscoveryErrorCallback(
      size_t index);

  const base::Closure& GetStopDiscoveryCallback(size_t index);
  const device::BluetoothDiscoverySession::ErrorCallback&
  GetStopDiscoveryErrorCallback(size_t index);

 protected:
  void ProcessQueue() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBleSynchronizer);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_BLE_SYNCHRONIZER_H_
