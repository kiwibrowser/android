// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_radio_winrt.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/async_operation.h"

namespace device {

namespace {

using ABI::Windows::Devices::Radios::Radio;
using ABI::Windows::Devices::Radios::RadioAccessStatus;
using ABI::Windows::Devices::Radios::RadioAccessStatus_Allowed;
using ABI::Windows::Devices::Radios::RadioKind;
using ABI::Windows::Devices::Radios::RadioState;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Foundation::IAsyncOperation;
using ABI::Windows::Foundation::ITypedEventHandler;
using Microsoft::WRL::Make;

}  // namespace

FakeRadioWinrt::FakeRadioWinrt() = default;

FakeRadioWinrt::~FakeRadioWinrt() = default;

HRESULT FakeRadioWinrt::SetStateAsync(
    RadioState value,
    IAsyncOperation<RadioAccessStatus>** operation) {
  state_ = value;
  auto async_op = Make<base::win::AsyncOperation<RadioAccessStatus>>();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(async_op->callback(), RadioAccessStatus_Allowed));
  *operation = async_op.Detach();
  return S_OK;
}

HRESULT FakeRadioWinrt::add_StateChanged(
    ITypedEventHandler<Radio*, IInspectable*>* handler,
    EventRegistrationToken* event_cookie) {
  return E_NOTIMPL;
}

HRESULT FakeRadioWinrt::remove_StateChanged(
    EventRegistrationToken event_cookie) {
  return E_NOTIMPL;
}

HRESULT FakeRadioWinrt::get_State(RadioState* value) {
  *value = state_;
  return S_OK;
}

HRESULT FakeRadioWinrt::get_Name(HSTRING* value) {
  return E_NOTIMPL;
}

HRESULT FakeRadioWinrt::get_Kind(RadioKind* value) {
  return E_NOTIMPL;
}

FakeRadioStaticsWinrt::FakeRadioStaticsWinrt() = default;

FakeRadioStaticsWinrt::~FakeRadioStaticsWinrt() = default;

HRESULT FakeRadioStaticsWinrt::GetRadiosAsync(
    IAsyncOperation<IVectorView<Radio*>*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeRadioStaticsWinrt::GetDeviceSelector(HSTRING* device_selector) {
  return E_NOTIMPL;
}

HRESULT FakeRadioStaticsWinrt::FromIdAsync(HSTRING device_id,
                                           IAsyncOperation<Radio*>** value) {
  return E_NOTIMPL;
}

HRESULT FakeRadioStaticsWinrt::RequestAccessAsync(
    IAsyncOperation<RadioAccessStatus>** operation) {
  auto async_op = Make<base::win::AsyncOperation<RadioAccessStatus>>();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(async_op->callback(), RadioAccessStatus_Allowed));
  *operation = async_op.Detach();
  return S_OK;
}

}  // namespace device
