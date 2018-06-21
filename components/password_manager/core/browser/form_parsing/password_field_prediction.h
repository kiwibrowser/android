// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_PASSWORD_FIELD_PREDICTION_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_PASSWORD_FIELD_PREDICTION_H_

#include <stdint.h>
#include <map>

#include "components/autofill/core/browser/field_types.h"

namespace autofill {
struct FormData;
class FormStructure;
}  // namespace autofill

namespace password_manager {

enum class CredentialFieldType {
  kNone,
  kUsername,
  kCurrentPassword,
  kNewPassword,
  kConfirmationPassword
};

// Transforms the general field type to the information useful for password
// forms.
CredentialFieldType DeriveFromServerFieldType(autofill::ServerFieldType type);

// Contains server predictions for a field.
// This is the struct rather than using because it will be expanded soon with
// additional information.
// TODO(https://crbug.com/831123): Remove comment about struct usage purposes as
// soon as additional fields added.
struct PasswordFieldPrediction {
  autofill::ServerFieldType type;
};

// Contains server predictions for a form. Keys are unique renderer ids of
// fields.
using FormPredictions = std::map<uint32_t, PasswordFieldPrediction>;

// Extracts all password related server predictions from |form_structure|.
// |observed_form| and |form_structure| must correspond to the same form.
FormPredictions ConvertToFormPredictions(
    const autofill::FormData& observed_form,
    const autofill::FormStructure& form_structure);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_PASSWORD_FIELD_PREDICTION_H_
