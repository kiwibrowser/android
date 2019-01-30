// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
#define CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_

#include "base/observer_list.h"
#include "chrome/browser/webauthn/transport_list_model.h"

// Encapsulates the model behind the Web Authentication request dialog's UX
// flow. This is essentially a state machine going through the states defined in
// the `Step` enumeration.
//
// Ultimately, this will become an observer of the AuthenticatorRequest, and
// contain the logic to figure out which steps the user needs to take, in which
// order, to complete the authentication flow.
class AuthenticatorRequestDialogModel {
 public:
  // Defines the potential steps of the Web Authentication API request UX flow.
  enum class Step {
    kInitial,
    kTransportSelection,
    kErrorTimedOut,
    kCompleted,

    // Universal Serial Bus (USB).
    kUsbInsert,
    kUsbActivate,
    kUsbVerifying,

    // Bluetooth Low Energy (BLE).
    kBlePowerOnAutomatic,
    kBlePowerOnManual,

    kBlePairingBegin,
    kBleEnterPairingMode,
    kBleDeviceSelection,
    kBlePinEntry,

    kBleActivate,
    kBleVerifying,
  };

  // Implemented by the dialog to observe this model and show the UI panels
  // appropriate for the current step.
  class Observer {
   public:
    // Called just before the model is destructed.
    virtual void OnModelDestroyed() = 0;

    // Called when the UX flow has navigated to a different step, so the UI
    // should update.
    virtual void OnStepTransition() {}
  };

  AuthenticatorRequestDialogModel();
  ~AuthenticatorRequestDialogModel();

  void SetCurrentStep(Step step);
  Step current_step() const { return current_step_; }

  TransportListModel* transport_list_model() { return &transport_list_model_; }

  // Requests that the step-by-step wizard flow commence, guiding the user
  // through using the Secutity Key with the given |transport|.
  //
  // Valid action when at step: kTransportSelection.
  void StartGuidedFlowForTransport(AuthenticatorTransport transport);

  // Tries if the BLE adapter is now powered -- the user claims they turned it
  // on.
  //
  // Valid action when at step: kBlePowerOnManual.
  void TryIfBleAdapterIsPowered();

  // Turns on the BLE adapter automatically.
  //
  // Valid action when at step: kBlePowerOnAutomatic.
  void PowerOnBleAdapter();

  // Lets the pairing procedure start after the user learned about the need.
  //
  // Valid action when at step: kBlePairingBegin.
  void StartBleDiscovery();

  // Initiates pairing of the device that the user has chosen.
  //
  // Valid action when at step: kBleDeviceSelection.
  void InitiatePairingDevice(const std::string& device_address);

  // Finishes pairing of the previously chosen device with the |pin| code
  // entered.
  //
  // Valid action when at step: kBlePinEntry.
  void FinishPairingWithPin(const base::string16& pin);

  // Tries if a USB device is present -- the user claims they plugged it in.
  //
  // Valid action when at step: kUsbInsert.
  void TryUsbDevice();

  // Cancels the flow as a result of the user clicking `Cancel` on the UI.
  //
  // Valid action at all steps.
  void Cancel();

  // Backtracks in the flow as a result of the user clicking `Back` on the UI.
  //
  // Valid action at all steps.
  void Back();

  // The |observer| must either outlive the object, or unregister itself on its
  // destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // To be called when the Web Authentication request is complete.
  void OnRequestComplete();

 private:
  // The current step of the request UX flow that is currently shown.
  Step current_step_ = Step::kInitial;

  TransportListModel transport_list_model_;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestDialogModel);
};

#endif  // CHROME_BROWSER_WEBAUTHN_AUTHENTICATOR_REQUEST_DIALOG_MODEL_H_
