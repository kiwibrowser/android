// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include "base/logging.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// AuthenticatorSheetModelBase ------------------------------------------------

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model)
    : dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->AddObserver(this);
}

AuthenticatorSheetModelBase::~AuthenticatorSheetModelBase() {
  if (dialog_model_) {
    dialog_model_->RemoveObserver(this);
    dialog_model_ = nullptr;
  }
}

bool AuthenticatorSheetModelBase::IsBackButtonVisible() const {
  return true;
}

bool AuthenticatorSheetModelBase::IsCancelButtonVisible() const {
  return true;
}

base::string16 AuthenticatorSheetModelBase::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorSheetModelBase::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsAcceptButtonEnabled() const {
  NOTREACHED();
  return false;
}

base::string16 AuthenticatorSheetModelBase::GetAcceptButtonLabel() const {
  NOTREACHED();
  return base::string16();
}

void AuthenticatorSheetModelBase::OnBack() {
  if (dialog_model())
    dialog_model()->Back();
}

void AuthenticatorSheetModelBase::OnAccept() {
  NOTREACHED();
}

void AuthenticatorSheetModelBase::OnCancel() {
  if (dialog_model())
    dialog_model()->Cancel();
}

void AuthenticatorSheetModelBase::OnModelDestroyed() {
  dialog_model_ = nullptr;
}

// AuthenticatorInitialSheetModel ---------------------------------------------

base::string16 AuthenticatorInitialSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_TITLE);
}

base::string16 AuthenticatorInitialSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_DESCRIPTION);
}

// AuthenticatorTransportSelectorSheetModel -----------------------------------

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_TITLE);
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_DIALOG_DESCRIPTION);
}

void AuthenticatorTransportSelectorSheetModel::OnTransportSelected(
    AuthenticatorTransport transport) {
  dialog_model()->StartGuidedFlowForTransport(transport);
}
