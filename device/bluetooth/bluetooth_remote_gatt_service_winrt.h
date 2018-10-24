// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WINRT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"

namespace device {

class BluetoothUUID;
class BluetoothDevice;
class BluetoothRemoteGattCharacteristic;

class DEVICE_BLUETOOTH_EXPORT BluetoothRemoteGattServiceWinrt
    : public BluetoothRemoteGattService {
 public:
  BluetoothRemoteGattServiceWinrt();
  ~BluetoothRemoteGattServiceWinrt() override;

  // BluetoothRemoteGattService:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  bool IsPrimary() const override;
  BluetoothDevice* GetDevice() const override;
  std::vector<BluetoothRemoteGattCharacteristic*> GetCharacteristics()
      const override;
  std::vector<BluetoothRemoteGattService*> GetIncludedServices() const override;
  BluetoothRemoteGattCharacteristic* GetCharacteristic(
      const std::string& identifier) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothRemoteGattServiceWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVICE_WINRT_H_
