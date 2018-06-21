// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_interaction_model.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_ui_element.h"

namespace ash {

AssistantInteractionModel::AssistantInteractionModel()
    : committed_query_(std::make_unique<AssistantEmptyQuery>()),
      pending_query_(std::make_unique<AssistantEmptyQuery>()) {}

AssistantInteractionModel::~AssistantInteractionModel() = default;

void AssistantInteractionModel::AddObserver(
    AssistantInteractionModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantInteractionModel::RemoveObserver(
    AssistantInteractionModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantInteractionModel::ClearInteraction(bool retain_committed_query) {
  if (!retain_committed_query)
    ClearCommittedQuery();

  ClearPendingQuery();
  ClearUiElements();
  ClearSuggestions();
}

void AssistantInteractionModel::SetInteractionState(
    InteractionState interaction_state) {
  if (interaction_state == interaction_state_)
    return;

  interaction_state_ = interaction_state;
  NotifyInteractionStateChanged();
}

void AssistantInteractionModel::SetInputModality(InputModality input_modality) {
  if (input_modality == input_modality_)
    return;

  input_modality_ = input_modality;
  NotifyInputModalityChanged();
}

void AssistantInteractionModel::SetMicState(MicState mic_state) {
  if (mic_state == mic_state_)
    return;

  mic_state_ = mic_state;
  NotifyMicStateChanged();
}

void AssistantInteractionModel::ClearCommittedQuery() {
  committed_query_ = std::make_unique<AssistantEmptyQuery>();
  NotifyCommittedQueryCleared();
}

void AssistantInteractionModel::SetPendingQuery(
    std::unique_ptr<AssistantQuery> pending_query) {
  DCHECK(!pending_query->Empty());
  pending_query_ = std::move(pending_query);
  NotifyPendingQueryChanged();
}

void AssistantInteractionModel::CommitPendingQuery() {
  committed_query_ = std::move(pending_query_);
  pending_query_ = std::make_unique<AssistantEmptyQuery>();

  NotifyCommittedQueryChanged();
  NotifyPendingQueryCleared();
}

void AssistantInteractionModel::ClearPendingQuery() {
  pending_query_ = std::make_unique<AssistantEmptyQuery>();
  NotifyPendingQueryCleared();
}

void AssistantInteractionModel::AddUiElement(
    std::unique_ptr<AssistantUiElement> ui_element) {
  AssistantUiElement* ptr = ui_element.get();
  ui_element_list_.push_back(std::move(ui_element));
  NotifyUiElementAdded(ptr);
}

void AssistantInteractionModel::ClearUiElements() {
  ui_element_list_.clear();
  NotifyUiElementsCleared();
}

void AssistantInteractionModel::AddSuggestions(
    std::vector<AssistantSuggestionPtr> suggestions) {
  std::map<int, AssistantSuggestion*> ptrs;

  // We use vector index to uniquely identify a given suggestion. This means
  // that suggestion ids will reset with each call to |ClearSuggestions|, but
  // that is acceptable.
  for (AssistantSuggestionPtr& suggestion : suggestions) {
    int id = suggestions_.size();
    suggestions_.push_back(std::move(suggestion));
    ptrs[id] = suggestions_.back().get();
  }

  NotifySuggestionsAdded(ptrs);
}

const AssistantInteractionModel::AssistantSuggestion*
AssistantInteractionModel::GetSuggestionById(int id) const {
  return id >= 0 && id < static_cast<int>(suggestions_.size())
             ? suggestions_.at(id).get()
             : nullptr;
}

void AssistantInteractionModel::ClearSuggestions() {
  suggestions_.clear();
  NotifySuggestionsCleared();
}

void AssistantInteractionModel::SetSpeechLevel(float speech_level_db) {
  NotifySpeechLevelChanged(speech_level_db);
}

void AssistantInteractionModel::NotifyInteractionStateChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnInteractionStateChanged(interaction_state_);
}

void AssistantInteractionModel::NotifyInputModalityChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnInputModalityChanged(input_modality_);
}

void AssistantInteractionModel::NotifyMicStateChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnMicStateChanged(mic_state_);
}

void AssistantInteractionModel::NotifyCommittedQueryChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnCommittedQueryChanged(*committed_query_);
}

void AssistantInteractionModel::NotifyCommittedQueryCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnCommittedQueryCleared();
}

void AssistantInteractionModel::NotifyPendingQueryChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnPendingQueryChanged(*pending_query_);
}

void AssistantInteractionModel::NotifyPendingQueryCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnPendingQueryCleared();
}

void AssistantInteractionModel::NotifyUiElementAdded(
    const AssistantUiElement* ui_element) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnUiElementAdded(ui_element);
}

void AssistantInteractionModel::NotifyUiElementsCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnUiElementsCleared();
}

void AssistantInteractionModel::NotifySuggestionsAdded(
    const std::map<int, AssistantSuggestion*>& suggestions) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnSuggestionsAdded(suggestions);
}

void AssistantInteractionModel::NotifySuggestionsCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnSuggestionsCleared();
}

void AssistantInteractionModel::NotifySpeechLevelChanged(
    float speech_level_db) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnSpeechLevelChanged(speech_level_db);
}

}  // namespace ash
