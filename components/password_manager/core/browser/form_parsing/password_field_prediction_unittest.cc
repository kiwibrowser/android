// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillField;
using autofill::ACCOUNT_CREATION_PASSWORD;
using autofill::CONFIRMATION_PASSWORD;
using autofill::EMAIL_ADDRESS;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormStructure;
using autofill::NEW_PASSWORD;
using autofill::NO_SERVER_DATA;
using autofill::PASSWORD;
using autofill::ServerFieldType;
using autofill::UNKNOWN_TYPE;
using autofill::USERNAME;
using autofill::USERNAME_AND_EMAIL_ADDRESS;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

TEST(FormPredictionsTest, ConvertToFormPredictions) {
  struct TestField {
    std::string name;
    std::string form_control_type;
    ServerFieldType input_type;
    ServerFieldType expected_type;
  } test_fields[] = {
      {"full_name", "text", UNKNOWN_TYPE, UNKNOWN_TYPE},
      // Password Manager is interested only in credential related types.
      {"Email", "email", EMAIL_ADDRESS, UNKNOWN_TYPE},
      {"username", "text", USERNAME, USERNAME},
      {"Password", "password", PASSWORD, PASSWORD},
      {"confirm_password", "password", CONFIRMATION_PASSWORD,
       CONFIRMATION_PASSWORD}};

  FormData form_data;
  for (size_t i = 0; i < base::size(test_fields); ++i) {
    FormFieldData field;
    field.unique_renderer_id = i + 1000;
    field.name = ASCIIToUTF16(test_fields[i].name);
    field.form_control_type = test_fields[i].form_control_type;
    form_data.fields.push_back(field);
  }

  FormStructure form_structure(form_data);
  size_t expected_predictions = 0;
  // Set server predictions and create expectected votes.
  for (size_t i = 0; i < base::size(test_fields); ++i) {
    AutofillField* field = form_structure.field(i);
    field->set_server_type(test_fields[i].input_type);
    ServerFieldType expected_type = test_fields[i].expected_type;
    if (expected_type != UNKNOWN_TYPE)
      ++expected_predictions;
  }

  FormPredictions actual_predictions =
      ConvertToFormPredictions(form_data, form_structure);

  // Check whether actual predictions are equal to expected ones.
  EXPECT_EQ(expected_predictions, actual_predictions.size());

  for (size_t i = 0; i < base::size(test_fields); ++i) {
    uint32_t unique_renderer_id = form_data.fields[i].unique_renderer_id;
    auto it = actual_predictions.find(unique_renderer_id);
    if (test_fields[i].expected_type == UNKNOWN_TYPE) {
      EXPECT_EQ(actual_predictions.end(), it);
    } else {
      ASSERT_NE(actual_predictions.end(), it);
      EXPECT_EQ(test_fields[i].expected_type, it->second.type);
    }
  }
}

TEST(FormPredictionsTest, DeriveFromServerFieldType) {
  struct TestCase {
    const char* name;
    // Input.
    ServerFieldType server_type;
    CredentialFieldType expected_result;
  } test_cases[] = {
      {"No prediction", NO_SERVER_DATA, CredentialFieldType::kNone},
      {"Irrelevant type", EMAIL_ADDRESS, CredentialFieldType::kNone},
      {"Username", USERNAME, CredentialFieldType::kUsername},
      {"Username/Email", USERNAME_AND_EMAIL_ADDRESS,
       CredentialFieldType::kUsername},
      {"Password", PASSWORD, CredentialFieldType::kCurrentPassword},
      {"New password", NEW_PASSWORD, CredentialFieldType::kNewPassword},
      {"Account creation password", ACCOUNT_CREATION_PASSWORD,
       CredentialFieldType::kNewPassword},
      {"Confirmation password", CONFIRMATION_PASSWORD,
       CredentialFieldType::kConfirmationPassword},
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    EXPECT_EQ(test_case.expected_result,
              DeriveFromServerFieldType(test_case.server_type));
  }
}

}  // namespace

}  // namespace password_manager
