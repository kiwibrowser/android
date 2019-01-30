// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

AuthenticatorRequestDialogModel::AuthenticatorRequestDialogModel() = default;
AuthenticatorRequestDialogModel::~AuthenticatorRequestDialogModel() {
  for (auto& observer : observers_)
    observer.OnModelDestroyed();
}

void AuthenticatorRequestDialogModel::SetCurrentStep(Step step) {
  current_step_ = step;
  for (auto& observer : observers_)
    observer.OnStepTransition();
}

void AuthenticatorRequestDialogModel::StartGuidedFlowForTransport(
    AuthenticatorTransport transport) {
  DCHECK_EQ(current_step(), Step::kTransportSelection);
  // TODO(engedy): Use transport here.
}

void AuthenticatorRequestDialogModel::TryIfBleAdapterIsPowered() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnManual);
}

void AuthenticatorRequestDialogModel::PowerOnBleAdapter() {
  DCHECK_EQ(current_step(), Step::kBlePowerOnAutomatic);
}

void AuthenticatorRequestDialogModel::StartBleDiscovery() {
  DCHECK_EQ(current_step(), Step::kBlePairingBegin);
}

void AuthenticatorRequestDialogModel::InitiatePairingDevice(
    const std::string& device_address) {
  DCHECK_EQ(current_step(), Step::kBleDeviceSelection);
}

void AuthenticatorRequestDialogModel::FinishPairingWithPin(
    const base::string16& pin) {
  DCHECK_EQ(current_step(), Step::kBlePinEntry);
}

void AuthenticatorRequestDialogModel::TryUsbDevice() {
  DCHECK_EQ(current_step(), Step::kUsbInsert);
}

void AuthenticatorRequestDialogModel::Cancel() {}

void AuthenticatorRequestDialogModel::Back() {}

void AuthenticatorRequestDialogModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthenticatorRequestDialogModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AuthenticatorRequestDialogModel::OnRequestComplete() {
  SetCurrentStep(Step::kCompleted);
}
