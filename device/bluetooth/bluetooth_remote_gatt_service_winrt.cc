// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service_winrt.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

BluetoothRemoteGattServiceWinrt::BluetoothRemoteGattServiceWinrt() = default;

BluetoothRemoteGattServiceWinrt::~BluetoothRemoteGattServiceWinrt() = default;

std::string BluetoothRemoteGattServiceWinrt::GetIdentifier() const {
  NOTIMPLEMENTED();
  return std::string();
}

BluetoothUUID BluetoothRemoteGattServiceWinrt::GetUUID() const {
  NOTIMPLEMENTED();
  return BluetoothUUID();
}

bool BluetoothRemoteGattServiceWinrt::IsPrimary() const {
  NOTIMPLEMENTED();
  return false;
}

BluetoothDevice* BluetoothRemoteGattServiceWinrt::GetDevice() const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattServiceWinrt::GetCharacteristics() const {
  NOTIMPLEMENTED();
  return {};
}

std::vector<BluetoothRemoteGattService*>
BluetoothRemoteGattServiceWinrt::GetIncludedServices() const {
  NOTIMPLEMENTED();
  return {};
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattServiceWinrt::GetCharacteristic(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace device
