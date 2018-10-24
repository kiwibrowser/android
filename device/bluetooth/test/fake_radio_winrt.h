// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_RADIO_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_RADIO_WINRT_H_

#include <windows.devices.radios.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/macros.h"

namespace device {

class FakeRadioWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Radios::IRadio> {
 public:
  FakeRadioWinrt();
  ~FakeRadioWinrt() override;

  // IRadio:
  IFACEMETHODIMP SetStateAsync(
      ABI::Windows::Devices::Radios::RadioState value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Radios::RadioAccessStatus>** operation)
      override;
  IFACEMETHODIMP add_StateChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Radios::Radio*,
          IInspectable*>* handler,
      EventRegistrationToken* event_cookie) override;
  IFACEMETHODIMP remove_StateChanged(
      EventRegistrationToken event_cookie) override;
  IFACEMETHODIMP get_State(
      ABI::Windows::Devices::Radios::RadioState* value) override;
  IFACEMETHODIMP get_Name(HSTRING* value) override;
  IFACEMETHODIMP get_Kind(
      ABI::Windows::Devices::Radios::RadioKind* value) override;

 private:
  ABI::Windows::Devices::Radios::RadioState state_ =
      ABI::Windows::Devices::Radios::RadioState_On;

  DISALLOW_COPY_AND_ASSIGN(FakeRadioWinrt);
};

class FakeRadioStaticsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Radios::IRadioStatics> {
 public:
  FakeRadioStaticsWinrt();
  ~FakeRadioStaticsWinrt() override;

  // IRadioStatics:
  IFACEMETHODIMP GetRadiosAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Foundation::Collections::IVectorView<
              ABI::Windows::Devices::Radios::Radio*>*>** value) override;
  IFACEMETHODIMP GetDeviceSelector(HSTRING* device_selector) override;
  IFACEMETHODIMP FromIdAsync(
      HSTRING device_id,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Radios::Radio*>** value) override;
  IFACEMETHODIMP RequestAccessAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Radios::RadioAccessStatus>** operation)
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRadioStaticsWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_RADIO_WINRT_H_
