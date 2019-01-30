// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_le_advertisement_winrt.h"

#include <utility>

#include "base/strings/string_piece.h"
#include "base/win/scoped_hstring.h"

namespace device {

namespace {

using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementDataSection;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementFlags;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEManufacturerData;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Foundation::Collections::IVectorView;
using Microsoft::WRL::ComPtr;

}  // namespace

FakeBluetoothLEAdvertisementWinrt::FakeBluetoothLEAdvertisementWinrt(
    base::Optional<std::string> local_name,
    ComPtr<IVector<GUID>> service_uuids)
    : local_name_(std::move(local_name)),
      service_uuids_(std::move(service_uuids)) {}

FakeBluetoothLEAdvertisementWinrt::~FakeBluetoothLEAdvertisementWinrt() =
    default;

HRESULT FakeBluetoothLEAdvertisementWinrt::get_Flags(
    IReference<BluetoothLEAdvertisementFlags>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::put_Flags(
    IReference<BluetoothLEAdvertisementFlags>* value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_LocalName(HSTRING* value) {
  if (!local_name_)
    return E_FAIL;

  *value = base::win::ScopedHString::Create(*local_name_).release();
  return S_OK;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::put_LocalName(HSTRING value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_ServiceUuids(
    IVector<GUID>** value) {
  return service_uuids_.CopyTo(value);
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_ManufacturerData(
    IVector<BluetoothLEManufacturerData*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::get_DataSections(
    IVector<BluetoothLEAdvertisementDataSection*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::GetManufacturerDataByCompanyId(
    uint16_t company_id,
    IVectorView<BluetoothLEManufacturerData*>** data_list) {
  return E_NOTIMPL;
}

HRESULT FakeBluetoothLEAdvertisementWinrt::GetSectionsByType(
    uint8_t type,
    IVectorView<BluetoothLEAdvertisementDataSection*>** section_list) {
  return E_NOTIMPL;
}

}  // namespace device
