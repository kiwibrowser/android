// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_DEVICE_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_DEVICE_WINRT_H_

#include <windows.devices.bluetooth.h>
#include <windows.foundation.h>
#include <wrl/implements.h>

#include <stdint.h>

#include "base/macros.h"

namespace device {

class FakeBluetoothLEDeviceWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice3> {
 public:
  FakeBluetoothLEDeviceWinrt();
  ~FakeBluetoothLEDeviceWinrt() override;

  // IBluetoothLEDevice:
  IFACEMETHODIMP get_DeviceId(HSTRING* value) override;
  IFACEMETHODIMP get_Name(HSTRING* value) override;
  IFACEMETHODIMP get_GattServices(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceService*>** value) override;
  IFACEMETHODIMP get_ConnectionStatus(
      ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus* value)
      override;
  IFACEMETHODIMP get_BluetoothAddress(uint64_t* value) override;
  IFACEMETHODIMP GetGattService(
      GUID service_uuid,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          IGattDeviceService** service) override;
  IFACEMETHODIMP add_NameChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_NameChanged(EventRegistrationToken token) override;
  IFACEMETHODIMP add_GattServicesChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_GattServicesChanged(
      EventRegistrationToken token) override;
  IFACEMETHODIMP add_ConnectionStatusChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_ConnectionStatusChanged(
      EventRegistrationToken token) override;

  // IBluetoothLEDevice3:
  IFACEMETHODIMP get_DeviceAccessInformation(
      ABI::Windows::Devices::Enumeration::IDeviceAccessInformation** value)
      override;
  IFACEMETHODIMP RequestAccessAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceAccessStatus>** operation)
      override;
  IFACEMETHODIMP GetGattServicesAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetGattServicesWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetGattServicesForUuidAsync(
      GUID service_uuid,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetGattServicesForUuidWithCacheModeAsync(
      GUID service_uuid,
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothLEDeviceWinrt);
};

class FakeBluetoothLEDeviceStaticsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics> {
 public:
  FakeBluetoothLEDeviceStaticsWinrt();
  ~FakeBluetoothLEDeviceStaticsWinrt() override;

  // IBluetoothLEDeviceStatics:
  IFACEMETHODIMP FromIdAsync(
      HSTRING device_id,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*>** operation)
      override;
  IFACEMETHODIMP FromBluetoothAddressAsync(
      uint64_t bluetooth_address,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*>** operation)
      override;
  IFACEMETHODIMP GetDeviceSelector(HSTRING* device_selector) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothLEDeviceStaticsWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_DEVICE_WINRT_H_
