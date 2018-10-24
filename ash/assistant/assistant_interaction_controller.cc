// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_interaction_controller.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/new_window_controller.h"
#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Toast -----------------------------------------------------------------------

constexpr int kToastDurationMs = 2500;
constexpr char kUnboundServiceToastId[] =
    "assistant_controller_unbound_service";

void ShowToast(const std::string& id, int message_id) {
  ToastData toast(id, l10n_util::GetStringUTF16(message_id), kToastDurationMs,
                  base::nullopt);
  Shell::Get()->toast_manager()->Show(toast);
}

}  // namespace

// AssistantInteractionController ----------------------------------------------

AssistantInteractionController::AssistantInteractionController()
    : assistant_event_subscriber_binding_(this) {
  AddModelObserver(this);
  Shell::Get()->highlighter_controller()->AddObserver(this);
}

AssistantInteractionController::~AssistantInteractionController() {
  Shell::Get()->highlighter_controller()->RemoveObserver(this);
  RemoveModelObserver(this);
}

void AssistantInteractionController::SetAssistant(
    chromeos::assistant::mojom::Assistant* assistant) {
  assistant_ = assistant;

  // Subscribe to Assistant events.
  chromeos::assistant::mojom::AssistantEventSubscriberPtr ptr;
  assistant_event_subscriber_binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_->AddAssistantEventSubscriber(std::move(ptr));
}

void AssistantInteractionController::SetAssistantSetup(
    mojom::AssistantSetup* assistant_setup) {
  assistant_setup_ = assistant_setup;
}

void AssistantInteractionController::AddModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.AddObserver(observer);
}

void AssistantInteractionController::RemoveModelObserver(
    AssistantInteractionModelObserver* observer) {
  assistant_interaction_model_.RemoveObserver(observer);
}

void AssistantInteractionController::StartInteraction() {
  if (!Shell::Get()->voice_interaction_controller()->setup_completed()) {
    assistant_setup_->StartAssistantOptInFlow();
    return;
  }

  if (!Shell::Get()->voice_interaction_controller()->settings_enabled())
    return;

  if (!assistant_) {
    ShowToast(kUnboundServiceToastId, IDS_ASH_ASSISTANT_ERROR_GENERIC);
    return;
  }

  OnInteractionStarted();
}

void AssistantInteractionController::StopInteraction() {
  assistant_interaction_model_.SetInteractionState(InteractionState::kInactive);
}

void AssistantInteractionController::ToggleInteraction() {
  if (assistant_interaction_model_.interaction_state() ==
      InteractionState::kInactive) {
    StartInteraction();
  } else {
    StopInteraction();
  }
}

void AssistantInteractionController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  if (interaction_state == InteractionState::kActive)
    return;

  // When the user-facing interaction is dismissed, we instruct the service to
  // terminate any listening, speaking, or pending query.
  has_active_interaction_ = false;
  assistant_->StopActiveInteraction();

  assistant_interaction_model_.ClearInteraction();
  assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
}

void AssistantInteractionController::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  // We clear the interaction when a query is committed, but need to retain
  // the committed query as it is query that is currently being fulfilled.
  assistant_interaction_model_.ClearInteraction(
      /*retain_committed_query=*/true);
}

void AssistantInteractionController::OnHighlighterEnabledChanged(
    HighlighterEnabledState state) {
  assistant_interaction_model_.SetInputModality(InputModality::kStylus);
  if (state == HighlighterEnabledState::kEnabled) {
    assistant_interaction_model_.SetInteractionState(InteractionState::kActive);
  } else if (state == HighlighterEnabledState::kDisabledByUser) {
    assistant_interaction_model_.SetInteractionState(
        InteractionState::kInactive);
  }
}

void AssistantInteractionController::OnInputModalityChanged(
    InputModality input_modality) {
  if (input_modality == InputModality::kVoice)
    return;

  // When switching to a non-voice input modality we instruct the underlying
  // service to terminate any listening, speaking, or pending voice query. We do
  // not do this when switching to voice input modality because initiation of a
  // voice interaction will automatically interrupt any pre-existing activity.
  // Stopping the active interaction here for voice input modality would
  // actually have the undesired effect of stopping the voice interaction.
  if (assistant_interaction_model_.pending_query().type() ==
      AssistantQueryType::kVoice) {
    has_active_interaction_ = false;
    assistant_->StopActiveInteraction();
    assistant_interaction_model_.ClearPendingQuery();
  }
}

void AssistantInteractionController::OnInteractionStarted() {
  has_active_interaction_ = true;
  assistant_interaction_model_.SetInteractionState(InteractionState::kActive);
}

void AssistantInteractionController::OnInteractionFinished(
    AssistantInteractionResolution resolution) {
  has_active_interaction_ = false;

  // When a voice query is interrupted we do not receive any follow up speech
  // recognition events but the mic is closed.
  if (resolution == AssistantInteractionResolution::kInterruption) {
    assistant_interaction_model_.SetMicState(MicState::kClosed);
  }
}

void AssistantInteractionController::OnHtmlResponse(
    const std::string& response) {
  if (!has_active_interaction_)
    return;

  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantCardElement>(response));
}

void AssistantInteractionController::OnSuggestionChipPressed(int id) {
  const AssistantSuggestion* suggestion =
      assistant_interaction_model_.GetSuggestionById(id);

  // If the suggestion contains a non-empty action url, we will handle the
  // suggestion chip pressed event by launching the action url in the browser.
  if (!suggestion->action_url.is_empty()) {
    OpenUrl(suggestion->action_url);
    return;
  }

  // Otherwise, we will submit a simple text query using the suggestion text.
  const std::string text = suggestion->text;

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(text));
  assistant_interaction_model_.CommitPendingQuery();

  assistant_->SendTextQuery(text);
}

void AssistantInteractionController::OnSuggestionsResponse(
    std::vector<AssistantSuggestionPtr> response) {
  if (!has_active_interaction_)
    return;

  assistant_interaction_model_.AddSuggestions(std::move(response));
}

void AssistantInteractionController::OnTextResponse(
    const std::string& response) {
  if (!has_active_interaction_)
    return;

  assistant_interaction_model_.AddUiElement(
      std::make_unique<AssistantTextElement>(response));
}

void AssistantInteractionController::OnSpeechRecognitionStarted() {
  assistant_interaction_model_.SetInputModality(InputModality::kVoice);
  assistant_interaction_model_.SetMicState(MicState::kOpen);
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>());
}

void AssistantInteractionController::OnSpeechRecognitionIntermediateResult(
    const std::string& high_confidence_text,
    const std::string& low_confidence_text) {
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>(high_confidence_text,
                                            low_confidence_text));
}

void AssistantInteractionController::OnSpeechRecognitionEndOfUtterance() {
  assistant_interaction_model_.SetMicState(MicState::kClosed);
}

void AssistantInteractionController::OnSpeechRecognitionFinalResult(
    const std::string& final_result) {
  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantVoiceQuery>(final_result));
  assistant_interaction_model_.CommitPendingQuery();
}

void AssistantInteractionController::OnSpeechLevelUpdated(float speech_level) {
  assistant_interaction_model_.SetSpeechLevel(speech_level);
}

void AssistantInteractionController::OnOpenUrlResponse(const GURL& url) {
  if (!has_active_interaction_)
    return;

  OpenUrl(url);
}

void AssistantInteractionController::OnOpenUrlFromTab(const GURL& url) {
  OpenUrl(url);
}

void AssistantInteractionController::OnDialogPlateButtonPressed(
    DialogPlateButtonId id) {
  if (id == DialogPlateButtonId::kKeyboardInputToggle) {
    assistant_interaction_model_.SetInputModality(InputModality::kKeyboard);
    return;
  }

  if (id != DialogPlateButtonId::kVoiceInputToggle) {
    return;
  }

  switch (assistant_interaction_model_.mic_state()) {
    case MicState::kClosed:
      assistant_->StartVoiceInteraction();
      break;
    case MicState::kOpen:
      has_active_interaction_ = false;
      assistant_->StopActiveInteraction();
      break;
  }
}

void AssistantInteractionController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  // TODO(dmblack): This case no longer makes sense now that the DialogPlate has
  // been rebuilt. Remove the ability to commit empty DialogPlate contents.
  if (text.empty())
    return;

  assistant_interaction_model_.SetPendingQuery(
      std::make_unique<AssistantTextQuery>(text));
  assistant_interaction_model_.CommitPendingQuery();

  assistant_->SendTextQuery(text);
}

void AssistantInteractionController::OpenUrl(const GURL& url) {
  Shell::Get()->new_window_controller()->NewTabWithUrl(url);
  StopInteraction();
}

}  // namespace ash
