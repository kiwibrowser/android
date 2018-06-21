// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_DEVICE_SERVICE_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_DEVICE_SERVICE_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.foundation.collections.h>
#include <wrl/implements.h>

#include <stdint.h>

#include "base/macros.h"

namespace device {

class FakeGattDeviceServiceWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDeviceService> {
 public:
  FakeGattDeviceServiceWinrt();
  ~FakeGattDeviceServiceWinrt() override;

  // IGattDeviceService:
  IFACEMETHODIMP GetCharacteristics(
      GUID characteristic_uuid,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristic*>** value) override;
  IFACEMETHODIMP GetIncludedServices(
      GUID service_uuid,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceService*>** value) override;
  IFACEMETHODIMP get_DeviceId(HSTRING* value) override;
  IFACEMETHODIMP get_Uuid(GUID* value) override;
  IFACEMETHODIMP get_AttributeHandle(uint16_t* value) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGattDeviceServiceWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_DEVICE_SERVICE_WINRT_H_
