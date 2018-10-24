// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/fake_password_manager_client.h"

FakePasswordManagerClient::FakePasswordManagerClient() : binding_(this) {}

FakePasswordManagerClient::~FakePasswordManagerClient() {}

void FakePasswordManagerClient::BindRequest(
    autofill::mojom::PasswordManagerClientAssociatedRequest request) {
  binding_.Bind(std::move(request));
}

void FakePasswordManagerClient::Flush() {
  if (binding_.is_bound())
    binding_.FlushForTesting();
}

// autofill::mojom::PasswordManagerClient:
void FakePasswordManagerClient::AutomaticGenerationStatusChanged(
    bool available,
    const base::Optional<
        autofill::password_generation::PasswordGenerationUIData>& ui_data) {
  if (available) {
    called_automatic_generation_status_changed_true_ = true;
  }
}

void FakePasswordManagerClient::ShowManualPasswordGenerationPopup(
    const autofill::password_generation::PasswordGenerationUIData& ui_data) {
  called_show_manual_pw_generation_popup_ = true;
}

void FakePasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form) {}

void FakePasswordManagerClient::GenerationAvailableForForm(
    const autofill::PasswordForm& form) {
  called_generation_available_for_form_ = true;
}

void FakePasswordManagerClient::PasswordGenerationRejectedByTyping() {
  called_password_generation_rejected_by_typing_ = true;
}
