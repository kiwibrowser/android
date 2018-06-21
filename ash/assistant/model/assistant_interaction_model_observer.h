// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"

namespace chromeos {
namespace assistant {
namespace mojom {
class AssistantSuggestion;
}  // namespace mojom
}  // namespace assistant
}  // namespace chromeos

namespace ash {

class AssistantQuery;
class AssistantUiElement;
enum class InputModality;
enum class InteractionState;
enum class MicState;

// An observer which receives notification of changes to an Assistant
// interaction.
class AssistantInteractionModelObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  // Invoked when the interaction state is changed.
  virtual void OnInteractionStateChanged(InteractionState interaction_state) {}

  // Invoked when the input modality associated with the interaction is changed.
  virtual void OnInputModalityChanged(InputModality input_modality) {}

  // Invoked when the mic state associated with the interaction is changed.
  virtual void OnMicStateChanged(MicState mic_state) {}

  // Invoked when the committed query associated with the interaction is
  // changed.
  virtual void OnCommittedQueryChanged(const AssistantQuery& committed_query) {}

  // Invoked when the committed query associated with the interaction is
  // cleared.
  virtual void OnCommittedQueryCleared() {}

  // Invoked when the pending query associated with the interaction is changed.
  virtual void OnPendingQueryChanged(const AssistantQuery& pending_query) {}

  // Invoked when the pending query associated with the interaction is cleared.
  virtual void OnPendingQueryCleared() {}

  // Invoked when a UI element associated with the interaction is added.
  virtual void OnUiElementAdded(const AssistantUiElement* ui_element) {}

  // Invoked when all UI elements associated with the interaction are cleared.
  virtual void OnUiElementsCleared() {}

  // Invoked when the specified |suggestions| are added to the associated
  // interaction. The key for the map is the unique identifier by which the
  // interaction model identifies each suggestion before the next
  // |OnSuggestionsCleared| call.
  virtual void OnSuggestionsAdded(
      const std::map<int, AssistantSuggestion*>& suggestions) {}

  // Invoked when all suggestions associated with the interaction are cleared.
  virtual void OnSuggestionsCleared() {}

  // Invoked when the speech level is changed.
  virtual void OnSpeechLevelChanged(float speech_level_db) {}

 protected:
  AssistantInteractionModelObserver() = default;
  virtual ~AssistantInteractionModelObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionModelObserver);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
