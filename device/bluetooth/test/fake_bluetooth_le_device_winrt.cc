// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_device_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::BluetoothCacheMode;
using ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus;
using ABI::Windows::Devices::Bluetooth::BluetoothLEDevice;
using ABI::Windows::Devices::Enumeration::DeviceAccessStatus;
using ABI::Windows::Devices::Enumeration::IDeviceAccessInformation;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceServicesResult;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    IGattDeviceService;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;

}  // namespace

FakeBluetoothLEDeviceWinrt::FakeBluetoothLEDeviceWinrt() = default;

FakeBluetoothLEDeviceWinrt::~FakeBluetoothLEDeviceWinrt() = default;

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceId(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_Name(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_GattServices(
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_ConnectionStatus(
    BluetoothConnectionStatus* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_BluetoothAddress(uint64_t* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattService(
    GUID service_uuid,
    IGattDeviceService** service) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_NameChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_NameChanged(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_GattServicesChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_GattServicesChanged(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::add_ConnectionStatusChanged(
    ITypedEventHandler<BluetoothLEDevice*, IInspectable*>* handler,
    EventRegistrationToken* token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::remove_ConnectionStatusChanged(
    EventRegistrationToken token) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::get_DeviceAccessInformation(
    IDeviceAccessInformation** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::RequestAccessAsync(
    IAsyncOperation<DeviceAccessStatus>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesAsync(
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesWithCacheModeAsync(
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesForUuidAsync(
    GUID service_uuid,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceWinrt::GetGattServicesForUuidWithCacheModeAsync(
    GUID service_uuid,
    BluetoothCacheMode cache_mode,
    IAsyncOperation<GattDeviceServicesResult*>** operation) {
  return E_NOTIMPL;
}

FakeBluetoothLEDeviceStaticsWinrt::FakeBluetoothLEDeviceStaticsWinrt() =
    default;

FakeBluetoothLEDeviceStaticsWinrt::~FakeBluetoothLEDeviceStaticsWinrt() =
    default;

HRESULT FakeBluetoothLEDeviceStaticsWinrt::FromIdAsync(
    HSTRING device_id,
    IAsyncOperation<BluetoothLEDevice*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceStaticsWinrt::FromBluetoothAddressAsync(
    uint64_t bluetooth_address,
    IAsyncOperation<BluetoothLEDevice*>** operation) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEDeviceStaticsWinrt::GetDeviceSelector(
    HSTRING* device_selector) {
  return E_NOTIMPL;
}

}  // namespace device
