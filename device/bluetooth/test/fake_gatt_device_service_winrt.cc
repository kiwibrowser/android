// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_device_service_winrt.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCharacteristic;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Foundation::Collections::IVectorView;

}  // namespace

FakeGattDeviceServiceWinrt::FakeGattDeviceServiceWinrt() = default;

FakeGattDeviceServiceWinrt::~FakeGattDeviceServiceWinrt() = default;

HRESULT FakeGattDeviceServiceWinrt::GetCharacteristics(
    GUID characteristic_uuid,
    IVectorView<GattCharacteristic*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::GetIncludedServices(
    GUID service_uuid,
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_DeviceId(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_Uuid(GUID* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServiceWinrt::get_AttributeHandle(uint16_t* value) {
  return E_NOTIMPL;
}

}  // namespace device
