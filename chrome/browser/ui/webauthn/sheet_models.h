// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
#define CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_

#include "base/macros.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/transport_list_model.h"

// Base class for sheets, implementing the shared behavior used on most sheets,
// as well as maintaining a weak pointer to the dialog model.
class AuthenticatorSheetModelBase
    : public AuthenticatorRequestSheetModel,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit AuthenticatorSheetModelBase(
      AuthenticatorRequestDialogModel* dialog_model);
  ~AuthenticatorSheetModelBase() override;

  AuthenticatorRequestDialogModel* dialog_model() const {
    return dialog_model_;
  }

 protected:
  // AuthenticatorRequestSheetModel:
  bool IsBackButtonVisible() const override;
  bool IsCancelButtonVisible() const override;
  base::string16 GetCancelButtonLabel() const override;
  bool IsAcceptButtonVisible() const override;
  bool IsAcceptButtonEnabled() const override;
  base::string16 GetAcceptButtonLabel() const override;
  void OnBack() override;
  void OnAccept() override;
  void OnCancel() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;

 private:
  AuthenticatorRequestDialogModel* dialog_model_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorSheetModelBase);
};

// The initial sheet shown when the UX flow starts.
class AuthenticatorInitialSheetModel : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

 private:
  // AuthenticatorSheetModelBase:
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

// The sheet shown for selecting the transport over which the security key
// should be accessed.
class AuthenticatorTransportSelectorSheetModel
    : public AuthenticatorSheetModelBase {
 public:
  using AuthenticatorSheetModelBase::AuthenticatorSheetModelBase;

  // Initiates the step-by-step flow with the the transport at the given |index|
  // selected by the user.
  void OnTransportSelected(AuthenticatorTransport transport);

 private:
  // AuthenticatorSheetModelBase:
  base::string16 GetStepTitle() const override;
  base::string16 GetStepDescription() const override;
};

#endif  // CHROME_BROWSER_UI_WEBAUTHN_SHEET_MODELS_H_
