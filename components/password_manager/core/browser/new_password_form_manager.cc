// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/new_password_form_manager.h"

#include "components/autofill/core/browser/form_structure.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/form_fetcher_impl.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/password_form_filling.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_util.h"

using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using base::TimeDelta;

using Logger = autofill::SavePasswordProgressLogger;

namespace password_manager {

namespace {

constexpr TimeDelta kMaxFillingDelayForServerPerdictions =
    TimeDelta::FromMilliseconds(500);

// Helper function for calling form parsing and logging results if logging is
// active.
std::unique_ptr<autofill::PasswordForm> ParseFormAndMakeLogging(
    PasswordManagerClient* client,
    const FormData& form,
    const base::Optional<FormPredictions>& predictions) {
  std::unique_ptr<autofill::PasswordForm> password_form =
      ParseFormData(form, predictions ? &predictions.value() : nullptr,
                    FormParsingMode::FILLING);

  if (password_manager_util::IsLoggingActive(client)) {
    BrowserSavePasswordProgressLogger logger(client->GetLogManager());
    logger.LogFormData(Logger::STRING_FORM_PARSING_INPUT, form);
    if (password_form)
      logger.LogPasswordForm(Logger::STRING_FORM_PARSING_OUTPUT,
                             *password_form);
  }
  return password_form;
}

}  // namespace

NewPasswordFormManager::NewPasswordFormManager(
    PasswordManagerClient* client,
    const base::WeakPtr<PasswordManagerDriver>& driver,
    const FormData& observed_form,
    FormFetcher* form_fetcher)
    : client_(client),
      driver_(driver),
      observed_form_(observed_form),
      owned_form_fetcher_(
          form_fetcher ? nullptr
                       : std::make_unique<FormFetcherImpl>(
                             PasswordStore::FormDigest(observed_form),
                             client_,
                             true /* should_migrate_http_passwords */,
                             true /* should_query_suppressed_https_forms */)),
      form_fetcher_(form_fetcher ? form_fetcher : owned_form_fetcher_.get()),
      weak_ptr_factory_(this) {
  metrics_recorder_ = base::MakeRefCounted<PasswordFormMetricsRecorder>(
      client_->IsMainFrameSecure(), client_->GetUkmSourceId());
  metrics_recorder_->RecordFormSignature(CalculateFormSignature(observed_form));

  if (owned_form_fetcher_)
    owned_form_fetcher_->Fetch();
  form_fetcher_->AddConsumer(this);

  // The folloing code is for development and debugging purposes.
  // TODO(https://crbug.com/831123): remove it when NewPasswordFormManager will
  // be production ready.
  if (password_manager_util::IsLoggingActive(client_))
    ParseFormAndMakeLogging(client_, observed_form_, predictions_);
}
NewPasswordFormManager::~NewPasswordFormManager() = default;

bool NewPasswordFormManager::DoesManage(
    const autofill::FormData& form,
    const PasswordManagerDriver* driver) const {
  if (driver != driver_.get())
    return false;
  if (observed_form_.is_form_tag != form.is_form_tag)
    return false;
  // All unowned input elements are considered as one synthetic form.
  if (!observed_form_.is_form_tag && !form.is_form_tag)
    return true;
  return observed_form_.unique_renderer_id == form.unique_renderer_id;
}

FormFetcher* NewPasswordFormManager::GetFormFetcher() {
  return form_fetcher_;
}

const GURL& NewPasswordFormManager::GetOrigin() const {
  return observed_form_.origin;
}

const std::map<base::string16, const autofill::PasswordForm*>&
NewPasswordFormManager::GetBestMatches() const {
  return best_matches_;
}

const autofill::PasswordForm& NewPasswordFormManager::GetPendingCredentials()
    const {
  // TODO(https://crbug.com/831123): Implement.
  DCHECK(false);
  static autofill::PasswordForm dummy_form;
  return dummy_form;
}

metrics_util::CredentialSourceType
NewPasswordFormManager::GetCredentialSource() {
  // TODO(https://crbug.com/831123): Implement.
  return metrics_util::CredentialSourceType::kPasswordManager;
}

PasswordFormMetricsRecorder* NewPasswordFormManager::GetMetricsRecorder() {
  return metrics_recorder_.get();
}

const std::vector<const autofill::PasswordForm*>&
NewPasswordFormManager::GetBlacklistedMatches() const {
  // TODO(https://crbug.com/831123): Implement.
  DCHECK(false);
  static std::vector<const autofill::PasswordForm*> dummy_vector;
  return dummy_vector;
}

bool NewPasswordFormManager::IsBlacklisted() const {
  // TODO(https://crbug.com/831123): Implement.
  return false;
}

bool NewPasswordFormManager::IsPasswordOverridden() const {
  // TODO(https://crbug.com/831123): Implement.
  return false;
}

const autofill::PasswordForm* NewPasswordFormManager::GetPreferredMatch()
    const {
  return preferred_match_;
}

// TODO(https://crbug.com/831123): Implement all methods from
// PasswordFormManagerForUI.
void NewPasswordFormManager::Save() {}
void NewPasswordFormManager::Update(
    const autofill::PasswordForm& credentials_to_update) {}
void NewPasswordFormManager::UpdateUsername(
    const base::string16& new_username) {}
void NewPasswordFormManager::UpdatePasswordValue(
    const base::string16& new_password) {}
void NewPasswordFormManager::OnNopeUpdateClicked() {}
void NewPasswordFormManager::OnNeverClicked() {}
void NewPasswordFormManager::OnNoInteraction(bool is_update) {}
void NewPasswordFormManager::PermanentlyBlacklist() {}
void NewPasswordFormManager::OnPasswordsRevealed() {}

void NewPasswordFormManager::ProcessMatches(
    const std::vector<const autofill::PasswordForm*>& non_federated,
    size_t filtered_count) {
  // TODO(https://crbug.com/831123). Implement correct treating of blacklisted
  // matches.
  std::vector<const autofill::PasswordForm*> not_best_matches;
  password_manager_util::FindBestMatches(non_federated, &best_matches_,
                                         &not_best_matches, &preferred_match_);
  filled_ = false;

  if (predictions_)
    Fill();
  else {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NewPasswordFormManager::Fill,
                       weak_ptr_factory_.GetWeakPtr()),
        kMaxFillingDelayForServerPerdictions);
  }
}

bool NewPasswordFormManager::SetSubmittedFormIfIsManaged(
    const autofill::FormData& submitted_form,
    const PasswordManagerDriver* driver) {
  if (!DoesManage(submitted_form, driver))
    return false;
  submitted_form_ = submitted_form;
  is_submitted_ = true;
  return true;
}

void NewPasswordFormManager::ProcessServerPredictions(
    const std::vector<FormStructure*>& predictions) {
  FormSignature observed_form_signature =
      CalculateFormSignature(observed_form_);
  for (const FormStructure* form_predictions : predictions) {
    if (form_predictions->form_signature() != observed_form_signature)
      continue;
    predictions_ = ConvertToFormPredictions(observed_form_, *form_predictions);
    Fill();
    break;
  }
}

void NewPasswordFormManager::Fill() {
  // There are additional signals (server-side data) and parse results in
  // filling and saving mode might be different so it is better not to cache
  // parse result, but to parse each time again.
  std::unique_ptr<autofill::PasswordForm> observed_password_form =
      ParseFormAndMakeLogging(client_, observed_form_, predictions_);
  if (!observed_password_form)
    return;

  RecordMetricOnCompareParsingResult(*observed_password_form);

  // TODO(https://crbug.com/831123). Move this lines to the beginning of the
  // function when the old parsing is removed.
  if (!driver_ || best_matches_.empty() || filled_)
    return;

  // Do not fill forms without password field.
  if (observed_password_form->password_element_renderer_id ==
      FormFieldData::kNotSetFormControlRendererId)
    return;

  // TODO(https://crbug.com/831123). Implement correct treating of federated
  // matches.
  std::vector<const autofill::PasswordForm*> federated_matches;
  SendFillInformationToRenderer(
      *client_, driver_.get(), false /* is_blacklisted */,
      *observed_password_form.get(), best_matches_, federated_matches,
      preferred_match_, metrics_recorder_.get());
  filled_ = true;
}

void NewPasswordFormManager::RecordMetricOnCompareParsingResult(
    const autofill::PasswordForm& parsed_form) {
  bool same =
      parsed_form.username_element == old_parsing_result_.username_element &&
      parsed_form.password_element == old_parsing_result_.password_element &&
      parsed_form.new_password_element ==
          old_parsing_result_.new_password_element &&
      parsed_form.confirmation_password_element ==
          old_parsing_result_.confirmation_password_element;
  if (same) {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kSame);
    return;
  }

  // In the old parsing for fields with empty name, placeholders are used. The
  // reason for this is that an empty "..._element" attribute in a PasswordForm
  // means that no corresponding input element exists. The new form parsing sets
  // empty string in that case because renderer ids are used instead of element
  // names for fields identification. Hence in case of anonymous fields, the
  // results will be different for sure. Compare to placeholders and record this
  // case.
  if (old_parsing_result_.username_element ==
          base::ASCIIToUTF16("anonymous_username") ||
      old_parsing_result_.password_element ==
          base::ASCIIToUTF16("anonymous_password") ||
      old_parsing_result_.new_password_element ==
          base::ASCIIToUTF16("anonymous_new_password") ||
      old_parsing_result_.confirmation_password_element ==
          base::ASCIIToUTF16("anonymous_confirmation_password")) {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kAnonymousFields);
  } else {
    metrics_recorder_->RecordParsingsComparisonResult(
        PasswordFormMetricsRecorder::ParsingComparisonResult::kDifferent);
  }
}

}  // namespace password_manager
