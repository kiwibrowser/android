// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <set>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"

using autofill::FieldPropertiesFlags;
using autofill::FormFieldData;
using autofill::PasswordForm;

namespace password_manager {

namespace {

constexpr char kAutocompleteUsername[] = "username";
constexpr char kAutocompleteCurrentPassword[] = "current-password";
constexpr char kAutocompleteNewPassword[] = "new-password";
constexpr char kAutocompleteCreditCardPrefix[] = "cc-";

// The susbset of autocomplete flags related to passwords.
enum class AutocompleteFlag {
  kNone,
  kUsername,
  kCurrentPassword,
  kNewPassword,
  // Represents the whole family of cc-* flags.
  kCreditCard
};

// The autocomplete attribute has one of the following structures:
//   [section-*] [shipping|billing] [type_hint] field_type
//   on | off | false
// (see
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#autofilling-form-controls%3A-the-autocomplete-attribute).
// For password forms, only the field_type is relevant. So parsing the attribute
// amounts to just taking the last token.  If that token is one of "username",
// "current-password" or "new-password", this returns an appropriate enum value.
// If the token starts with a "cc-" prefix, this returns kCreditCard.
// Otherwise, returns kNone.
AutocompleteFlag ExtractAutocompleteFlag(const std::string& attribute) {
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(attribute, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty())
    return AutocompleteFlag::kNone;

  const base::StringPiece& field_type = tokens.back();
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteUsername))
    return AutocompleteFlag::kUsername;
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteCurrentPassword))
    return AutocompleteFlag::kCurrentPassword;
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteNewPassword))
    return AutocompleteFlag::kNewPassword;

  if (base::StartsWith(field_type, kAutocompleteCreditCardPrefix,
                       base::CompareCase::SENSITIVE))
    return AutocompleteFlag::kCreditCard;

  return AutocompleteFlag::kNone;
}

// How likely is user interaction for a given field?
// Note: higher numeric values should match higher likeliness to allow using the
// standard operator< for comparison of likeliness.
enum class Interactability {
  // When the field is invisible.
  kUnlikely = 0,
  // When the field is visible/focusable.
  kPossible = 1,
  // When the user actually typed into the field before.
  kCertain = 2,
};

// A wrapper around FormFieldData, carrying some additional data used during
// parsing.
struct ProcessedField {
  // This points to the wrapped FormFieldData.
  const FormFieldData* field;

  // The flag derived from field->autocomplete_attribute.
  AutocompleteFlag autocomplete_flag = AutocompleteFlag::kNone;

  // True iff field->form_control_type == "password".
  bool is_password = false;

  Interactability interactability = Interactability::kUnlikely;
};

// Returns true iff |processed_field| matches the |interactability_bar|. That is
// when either:
// (1) |processed_field.interactability| is not less than |interactability_bar|,
//     or
// (2) |interactability_bar| is |kCertain|, and |processed_field| was
// autofilled. The second clause helps to handle the case when both Chrome and
// the user contribute to filling a form:
//
// <form>
//   <input type="password" autocomplete="current-password" id="Chrome">
//   <input type="password" autocomplete="new-password" id="user">
// </form>
//
// In the example above, imagine that Chrome filled the field with id=Chrome,
// and the user typed the new password in field with id=user. Then the parser
// should identify that id=Chrome is the current password and id=user is the new
// password. Without clause (2), Chrome would ignore id=Chrome.
bool MatchesInteractability(const ProcessedField& processed_field,
                            Interactability interactability_bar) {
  return (processed_field.interactability >= interactability_bar) ||
         (interactability_bar == Interactability::kCertain &&
          (processed_field.field->properties_mask &
           FieldPropertiesFlags::AUTOFILLED));
}

// Helper struct that is used to return results from the parsing function.
struct ParseResult {
  const FormFieldData* username_field = nullptr;
  const FormFieldData* password_field = nullptr;
  const FormFieldData* new_password_field = nullptr;
  const FormFieldData* confirmation_password_field = nullptr;

  bool IsEmpty() {
    return password_field == nullptr && new_password_field == nullptr;
  }
};

// Returns the first element of |fields| which has the specified
// |unique_renderer_id|, or null if there is no such element.
const FormFieldData* FindFieldWithUniqueRendererId(
    const std::vector<ProcessedField>& processed_fields,
    uint32_t unique_renderer_id) {
  for (const ProcessedField& processed_field : processed_fields) {
    if (processed_field.field->unique_renderer_id == unique_renderer_id)
      return processed_field.field;
  }
  return nullptr;
}

// Tries to parse |processed_fields| based on server |predictions|.
std::unique_ptr<ParseResult> ParseUsingPredictions(
    const std::vector<ProcessedField>& processed_fields,
    const FormPredictions& predictions) {
  auto result = std::make_unique<ParseResult>();
  // Note: The code does not check whether there is at most 1 username, 1
  // current password and at most 2 new passwords. It is assumed that server
  // side predictions are sane.
  for (const auto& prediction : predictions) {
    switch (DeriveFromServerFieldType(prediction.second.type)) {
      case CredentialFieldType::kUsername:
        result->username_field =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kCurrentPassword:
        result->password_field =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kNewPassword:
        result->new_password_field =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kConfirmationPassword:
        result->confirmation_password_field =
            FindFieldWithUniqueRendererId(processed_fields, prediction.first);
        break;
      case CredentialFieldType::kNone:
        break;
    }
  }
  return result->IsEmpty() ? nullptr : std::move(result);
}

// Tries to parse |processed_fields| based on autocomplete attributes.
// Assumption on the usage autocomplete attributes:
// 1. Not more than 1 field with autocomplete=username.
// 2. Not more than 1 field with autocomplete=current-password.
// 3. Not more than 2 fields with autocomplete=new-password.
// 4. Only password fields have "*-password" attribute and only non-password
//    fields have the "username" attribute.
// Are these assumptions violated, or is there no password with an autocomplete
// attribute, parsing is unsuccessful. Returns nullptr if parsing is
// unsuccessful.
std::unique_ptr<ParseResult> ParseUsingAutocomplete(
    const std::vector<ProcessedField>& processed_fields) {
  auto result = std::make_unique<ParseResult>();
  for (const ProcessedField& processed_field : processed_fields) {
    switch (processed_field.autocomplete_flag) {
      case AutocompleteFlag::kUsername:
        if (processed_field.is_password || result->username_field)
          return nullptr;
        result->username_field = processed_field.field;
        break;
      case AutocompleteFlag::kCurrentPassword:
        if (!processed_field.is_password || result->password_field)
          return nullptr;
        result->password_field = processed_field.field;
        break;
      case AutocompleteFlag::kNewPassword:
        if (!processed_field.is_password)
          return nullptr;
        // The first field with autocomplete=new-password is considered to be
        // new_password_field and the second is confirmation_password_field.
        if (!result->new_password_field)
          result->new_password_field = processed_field.field;
        else if (!result->confirmation_password_field)
          result->confirmation_password_field = processed_field.field;
        else
          return nullptr;
        break;
      case AutocompleteFlag::kCreditCard:
        NOTREACHED();
        break;
      case AutocompleteFlag::kNone:
        break;
    }
  }

  return result->IsEmpty() ? nullptr : std::move(result);
}

// Returns only relevant password fields from |processed_fields|. Namely, if
// |mode| == SAVING return only non-empty fields (for saving empty fields are
// useless). This ignores all passwords with Interactability below
// |best_interactability|. Stores the iterator to the first relevant password in
// |first_relevant_password|.
std::vector<const FormFieldData*> GetRelevantPasswords(
    const std::vector<ProcessedField>& processed_fields,
    FormParsingMode mode,
    Interactability best_interactability,
    std::vector<ProcessedField>::const_iterator* first_relevant_password) {
  DCHECK(first_relevant_password);
  *first_relevant_password = processed_fields.end();
  std::vector<const FormFieldData*> result;
  result.reserve(processed_fields.size());

  const bool consider_only_non_empty = mode == FormParsingMode::SAVING;

  for (auto it = processed_fields.begin(); it != processed_fields.end(); ++it) {
    const ProcessedField& processed_field = *it;
    if (!processed_field.is_password)
      continue;
    if (!MatchesInteractability(processed_field, best_interactability))
      continue;
    if (consider_only_non_empty && processed_field.field->value.empty())
      continue;
    // Readonly fields can be an indication that filling is useless (e.g., the
    // page might use a virtual keyboard). However, if the field was readonly
    // only temporarily, that makes it still interesting for saving. The fact
    // that a user typed or Chrome filled into that field in tha past is an
    // indicator that the radonly was only temporary.
    if (processed_field.field->is_readonly &&
        !(processed_field.field->properties_mask &
          (FieldPropertiesFlags::USER_TYPED |
           FieldPropertiesFlags::AUTOFILLED))) {
      continue;
    }
    if (*first_relevant_password == processed_fields.end())
      *first_relevant_password = it;
    result.push_back(processed_field.field);
  }

  return result;
}

// Detects different password fields from |passwords|.
void LocateSpecificPasswords(const std::vector<const FormFieldData*>& passwords,
                             const FormFieldData** current_password,
                             const FormFieldData** new_password,
                             const FormFieldData** confirmation_password) {
  DCHECK(current_password);
  DCHECK(!*current_password);
  DCHECK(new_password);
  DCHECK(!*new_password);
  DCHECK(confirmation_password);
  DCHECK(!*confirmation_password);

  switch (passwords.size()) {
    case 1:
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // If the passwords are both empty, it is impossible to tell if they
        // are the old and the new one, or the new one and its confirmation. In
        // that case Chrome errs on the side of filling and classifies them as
        // old & new to allow filling of change password forms.
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      // If there are more than 3 passwords it is not very clear what this form
      // it is. Consider only the first 3 passwords in such case as a
      // best-effort solution.
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value &&
          passwords[0]->value == passwords[2]->value) {
        // All passwords are the same. Assume that the first field is the
        // current password.
        *current_password = passwords[0];
      } else if (passwords[1]->value == passwords[2]->value) {
        // New password is the duplicated one, and comes second; or empty form
        // with at least 3 password fields.
        *current_password = passwords[0];
        *new_password = passwords[1];
        *confirmation_password = passwords[2];
      } else if (passwords[0]->value == passwords[1]->value) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields. Assume that
        // any password fields after the new password contain sensitive
        // information that isn't actually a password (security hint, SSN, etc.)
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which. Let's save the first password.
        // Password selection in a prompt will allow to correct the choice.
        *current_password = passwords[0];
      }
  }
}

// Tries to find username field among text fields from |processed_fields|
// occurring before |first_relevant_password|. Returns nullptr if the username
// is not found. If |mode| is SAVING, ignores all fields with empty values.
// Ignores all fields with interactability less than |best_interactability|.
const FormFieldData* FindUsernameFieldBaseHeuristics(
    const std::vector<ProcessedField>& processed_fields,
    const std::vector<ProcessedField>::const_iterator& first_relevant_password,
    FormParsingMode mode,
    Interactability best_interactability) {
  DCHECK(first_relevant_password != processed_fields.end());

  // For saving filter out empty fields.
  const bool consider_only_non_empty = mode == FormParsingMode::SAVING;

  // Search through the text input fields preceding |first_relevant_password|
  // and find the closest one focusable and the closest one in general.

  const FormFieldData* focusable_username = nullptr;
  const FormFieldData* username = nullptr;
  // Do reverse search to find the closest candidates preceding the password.
  for (auto it = std::make_reverse_iterator(first_relevant_password);
       it != processed_fields.rend(); ++it) {
    if (it->is_password)
      continue;
    if (!MatchesInteractability(*it, best_interactability))
      continue;
    if (consider_only_non_empty && it->field->value.empty())
      continue;
    if (!username)
      username = it->field;
    if (it->field->is_focusable) {
      focusable_username = it->field;
      break;
    }
  }

  return focusable_username ? focusable_username : username;
}

// Tries to find the username and password fields in |processed_fields| based on
// the structure (how the fields are ordered). If |mode| is SAVING, only
// consideres non-empty fields. If |username_hint| is not null, it is returned
// as the username.
std::unique_ptr<ParseResult> ParseUsingBaseHeuristics(
    const std::vector<ProcessedField>& processed_fields,
    FormParsingMode mode,
    const FormFieldData* username_hint) {
  // What is the best interactability among passwords?
  Interactability password_max = Interactability::kUnlikely;
  for (const ProcessedField& processed_field : processed_fields) {
    if (processed_field.is_password)
      password_max = std::max(password_max, processed_field.interactability);
  }

  // Try to find password elements (current, new, confirmation) among those with
  // best interactability.
  std::vector<ProcessedField>::const_iterator first_relevant_password =
      processed_fields.end();
  std::vector<const FormFieldData*> passwords = GetRelevantPasswords(
      processed_fields, mode, password_max, &first_relevant_password);
  if (passwords.empty())
    return nullptr;
  DCHECK(first_relevant_password != processed_fields.end());
  auto result = std::make_unique<ParseResult>();
  LocateSpecificPasswords(passwords, &result->password_field,
                          &result->new_password_field,
                          &result->confirmation_password_field);
  if (result->IsEmpty())
    return nullptr;

  if (username_hint &&
      !(mode == FormParsingMode::SAVING && username_hint->value.empty())) {
    result->username_field = username_hint;
    return result;
  }

  // What is the best interactability among text fields preceding the passwords?
  Interactability username_max = Interactability::kUnlikely;
  for (auto it = processed_fields.begin(); it != first_relevant_password;
       ++it) {
    if (!it->is_password)
      username_max = std::max(username_max, it->interactability);
  }

  // If password elements are found then try to find a username.
  result->username_field = FindUsernameFieldBaseHeuristics(
      processed_fields, first_relevant_password, mode, username_max);
  return result;
}

// Set username and password fields from |parse_result| in |password_form|.
void SetFields(const ParseResult& parse_result, PasswordForm* password_form) {
  password_form->has_renderer_ids = true;
  if (parse_result.username_field) {
    password_form->username_element = parse_result.username_field->name;
    password_form->username_value = parse_result.username_field->value;
    password_form->username_element_renderer_id =
        parse_result.username_field->unique_renderer_id;
  }

  if (parse_result.password_field) {
    password_form->password_element = parse_result.password_field->name;
    password_form->password_value = parse_result.password_field->value;
    password_form->password_element_renderer_id =
        parse_result.password_field->unique_renderer_id;
  }

  if (parse_result.new_password_field) {
    password_form->new_password_element = parse_result.new_password_field->name;
    password_form->new_password_value = parse_result.new_password_field->value;
  }

  if (parse_result.confirmation_password_field) {
    password_form->confirmation_password_element =
        parse_result.confirmation_password_field->name;
  }
}

// For each relevant field of |fields| computes additional data useful for
// parsing and wraps that in a ProcessedField. Returns the vector of all those
// ProcessedField instances, or an empty vector if there was not a single
// password field. Also, computes the vector of all password values and
// associated element names in |all_possible_passwords|.
std::vector<ProcessedField> ProcessFields(
    const std::vector<FormFieldData>& fields,
    autofill::ValueElementVector* all_possible_passwords) {
  DCHECK(all_possible_passwords);
  DCHECK(all_possible_passwords->empty());

  std::vector<ProcessedField> result;
  bool password_field_found = false;

  result.reserve(fields.size());

  // |all_possible_passwords| should only contain each value once. |seen_values|
  // ensures that duplicates are ignored.
  std::set<base::StringPiece16> seen_values;
  // Pretend that an empty value has been already seen, so that empty-valued
  // password elements won't get added to |all_possible_passwords|.
  seen_values.insert(base::StringPiece16());

  for (const FormFieldData& field : fields) {
    if (!field.IsTextInputElement())
      continue;

    const bool is_password = field.form_control_type == "password";
    if (is_password) {
      // Only the field name of the first occurrence is added to
      // |all_possible_passwords|.
      auto insertion = seen_values.insert(base::StringPiece16(field.value));
      if (insertion.second)  // There was no such element in |seen_values|.
        all_possible_passwords->push_back({field.value, field.name});
    }

    const AutocompleteFlag flag =
        ExtractAutocompleteFlag(field.autocomplete_attribute);
    if (flag == AutocompleteFlag::kCreditCard)
      continue;

    ProcessedField processed_field = {
        .field = &field, .autocomplete_flag = flag, .is_password = is_password};

    password_field_found |= is_password;

    if (field.properties_mask & FieldPropertiesFlags::USER_TYPED)
      processed_field.interactability = Interactability::kCertain;
    else if (field.is_focusable)
      processed_field.interactability = Interactability::kPossible;

    result.push_back(processed_field);
  }

  if (!password_field_found)
    result.clear();

  return result;
}

// Find the first element in |username_predictions| (i.e. the most reliable
// prediction) that occurs in |processed_fields|.
const FormFieldData* FindUsernameInPredictions(
    const std::vector<uint32_t>& username_predictions,
    const std::vector<ProcessedField>& processed_fields) {
  for (uint32_t predicted_id : username_predictions) {
    auto iter = std::find_if(
        processed_fields.begin(), processed_fields.end(),
        [predicted_id](const ProcessedField& processed_field) {
          return processed_field.field->unique_renderer_id == predicted_id;
        });
    if (iter != processed_fields.end()) {
      return iter->field;
    }
  }
  return nullptr;
}

}  // namespace

std::unique_ptr<PasswordForm> ParseFormData(
    const autofill::FormData& form_data,
    const FormPredictions* form_predictions,
    FormParsingMode mode) {
  autofill::ValueElementVector all_possible_passwords;
  std::vector<ProcessedField> processed_fields =
      ProcessFields(form_data.fields, &all_possible_passwords);

  if (processed_fields.empty())
    return nullptr;

  // Create parse result and set non-field related information.
  auto result = std::make_unique<PasswordForm>();
  result->origin = form_data.origin;
  result->signon_realm = form_data.origin.GetOrigin().spec();
  result->action = form_data.action;
  result->form_data = form_data;
  result->all_possible_passwords = std::move(all_possible_passwords);
  result->scheme = PasswordForm::SCHEME_HTML;
  result->preferred = false;
  result->blacklisted_by_user = false;
  result->type = PasswordForm::TYPE_MANUAL;

  if (form_predictions) {
    // Try to parse with server predictions.
    auto predictions_parse_result =
        ParseUsingPredictions(processed_fields, *form_predictions);
    if (predictions_parse_result) {
      SetFields(*predictions_parse_result, result.get());
      return result;
    }
  }

  // Try to parse with autocomplete attributes.
  auto autocomplete_parse_result = ParseUsingAutocomplete(processed_fields);
  if (autocomplete_parse_result) {
    SetFields(*autocomplete_parse_result, result.get());
    return result;
  }

  // Try to find the username based on the context of the fields.
  const FormFieldData* username_field_by_context = nullptr;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kHtmlBasedUsernameDetector)) {
    username_field_by_context = FindUsernameInPredictions(
        form_data.username_predictions, processed_fields);
  }

  // Try to parse with base heuristic.
  auto base_heuristics_parse_result = ParseUsingBaseHeuristics(
      processed_fields, mode, username_field_by_context);
  if (base_heuristics_parse_result) {
    SetFields(*base_heuristics_parse_result, result.get());
    return result;
  }

  return nullptr;
}

}  // namespace password_manager
