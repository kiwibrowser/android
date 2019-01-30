// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

#include "base/logging.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"

using autofill::FormData;
using autofill::FormStructure;
using autofill::ServerFieldType;

namespace password_manager {

namespace {

// Returns true if the field is password or username prediction.
bool IsCredentialRelatedPrediction(ServerFieldType type) {
  return DeriveFromServerFieldType(type) != CredentialFieldType::kNone;
}

}  // namespace

CredentialFieldType DeriveFromServerFieldType(ServerFieldType type) {
  switch (type) {
    case autofill::USERNAME:
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
      return CredentialFieldType::kUsername;
    case autofill::PASSWORD:
      return CredentialFieldType::kCurrentPassword;
    case autofill::ACCOUNT_CREATION_PASSWORD:
    case autofill::NEW_PASSWORD:
      return CredentialFieldType::kNewPassword;
    case autofill::CONFIRMATION_PASSWORD:
      return CredentialFieldType::kConfirmationPassword;
    default:
      return CredentialFieldType::kNone;
  }
}

FormPredictions ConvertToFormPredictions(const FormData& observed_form,
                                         const FormStructure& form_structure) {
  DCHECK_EQ(CalculateFormSignature(observed_form),
            form_structure.form_signature());
  DCHECK_EQ(observed_form.fields.size(), form_structure.field_count());
  FormPredictions result;
  if (observed_form.fields.size() != form_structure.field_count()) {
    // TODO(https://crbug.com/831123). Find the reason why this can happen. See
    // https://crbug.com/853149#c6 for some ideas.
    return result;
  }
  for (size_t i = 0; i < observed_form.fields.size(); ++i) {
    uint32_t unique_id = observed_form.fields[i].unique_renderer_id;
    ServerFieldType server_type = form_structure.field(i)->server_type();
    if (IsCredentialRelatedPrediction(server_type))
      result[unique_id] = PasswordFieldPrediction{.type = server_type};
  }

  return result;
}

}  // namespace password_manager
