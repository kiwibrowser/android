// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_gatt_device_services_result_winrt.h"

namespace device {

namespace {

using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattDeviceService;
using ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
    GattCommunicationStatus;

}  // namespace

FakeGattDeviceServicesResultWinrt::FakeGattDeviceServicesResultWinrt() =
    default;

FakeGattDeviceServicesResultWinrt::~FakeGattDeviceServicesResultWinrt() =
    default;

HRESULT FakeGattDeviceServicesResultWinrt::get_Status(
    GattCommunicationStatus* value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServicesResultWinrt::get_ProtocolError(
    IReference<uint8_t>** value) {
  return E_NOTIMPL;
}

HRESULT FakeGattDeviceServicesResultWinrt::get_Services(
    IVectorView<GattDeviceService*>** value) {
  return E_NOTIMPL;
}

}  // namespace device
