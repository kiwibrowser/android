// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_ui_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/ui/assistant_container_view.h"
#include "base/optional.h"

namespace ash {

AssistantUiController::AssistantUiController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_->interaction_controller()->AddModelObserver(this);
}

AssistantUiController::~AssistantUiController() {
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);

  if (container_view_)
    container_view_->GetWidget()->RemoveObserver(this);
}

void AssistantUiController::AddModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_ui_model_.AddObserver(observer);
}

void AssistantUiController::RemoveModelObserver(
    AssistantUiModelObserver* observer) {
  assistant_ui_model_.RemoveObserver(observer);
}

void AssistantUiController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active)
    container_view_->RequestFocus();
}

void AssistantUiController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  UpdateUiMode();
}

void AssistantUiController::OnWidgetDestroying(views::Widget* widget) {
  // We need to be sure that the Assistant interaction is stopped when the
  // widget is closed. Special cases, such as closing the widget via the |ESC|
  // key might otherwise go unhandled, causing inconsistencies between the
  // widget visibility state and the underlying interaction model state.
  // TODO(dmblack): Clean this up. Sibling controllers shouldn't need to
  // communicate to each other directly in this way.
  assistant_controller_->interaction_controller()->StopInteraction();

  container_view_->GetWidget()->RemoveObserver(this);
  container_view_ = nullptr;
}

void AssistantUiController::OnInputModalityChanged(
    InputModality input_modality) {
  UpdateUiMode();
}

void AssistantUiController::OnInteractionStateChanged(
    InteractionState interaction_state) {
  switch (interaction_state) {
    case InteractionState::kActive:
      Show();
      break;
    case InteractionState::kInactive:
      Dismiss();
      break;
  }
}

void AssistantUiController::OnMicStateChanged(MicState mic_state) {
  UpdateUiMode();
}

bool AssistantUiController::OnCaptionButtonPressed(CaptionButtonId id) {
  switch (id) {
    case CaptionButtonId::kMinimize:
      UpdateUiMode(AssistantUiMode::kMiniUi);
      return true;
    case CaptionButtonId::kClose:
      return false;
  }
  return false;
}

void AssistantUiController::OnDialogPlateButtonPressed(DialogPlateButtonId id) {
  if (id != DialogPlateButtonId::kSettings)
    return;

  UpdateUiMode(AssistantUiMode::kWebUi);
}

bool AssistantUiController::IsVisible() const {
  return container_view_ && container_view_->GetWidget()->IsVisible();
}

void AssistantUiController::Show() {
  if (!container_view_) {
    container_view_ = new AssistantContainerView(assistant_controller_);
    container_view_->GetWidget()->AddObserver(this);
  }
  container_view_->GetWidget()->Show();
}

void AssistantUiController::Dismiss() {
  if (container_view_)
    container_view_->GetWidget()->Hide();
}

void AssistantUiController::UpdateUiMode(
    base::Optional<AssistantUiMode> ui_mode) {
  // If a UI mode is provided, we will use it in lieu of updating UI mode on the
  // basis of interaction/widget visibility state.
  if (ui_mode.has_value()) {
    assistant_ui_model_.SetUiMode(ui_mode.value());
    return;
  }

  // When Assistant UI is not visible, we should reset to main UI mode.
  if (!IsVisible()) {
    assistant_ui_model_.SetUiMode(AssistantUiMode::kMainUi);
    return;
  }

  const AssistantInteractionModel* interaction_model =
      assistant_controller_->interaction_controller()->model();

  // When the mic is open, we should be in main UI mode.
  if (interaction_model->mic_state() == MicState::kOpen) {
    assistant_ui_model_.SetUiMode(AssistantUiMode::kMainUi);
    return;
  }

  // When stylus input modality is selected, we should be in mini UI mode.
  if (interaction_model->input_modality() == InputModality::kStylus) {
    assistant_ui_model_.SetUiMode(AssistantUiMode::kMiniUi);
    return;
  }

  // By default, we will fall back to main UI mode.
  assistant_ui_model_.SetUiMode(AssistantUiMode::kMainUi);
}

}  // namespace ash
