// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_

#include <deque>
#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantCardElement;
class AssistantController;
class AssistantHeaderView;
class AssistantTextElement;
class AssistantUiElement;

// UiElementContainerView is the child of AssistantMainView concerned with
// laying out text views and embedded card views in response to Assistant
// interaction model UI element events.
class UiElementContainerView : public views::View,
                               public AssistantInteractionModelObserver {
 public:
  explicit UiElementContainerView(AssistantController* assistant_controller);
  ~UiElementContainerView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantInteractionModelObserver:
  void OnUiElementAdded(const AssistantUiElement* ui_element) override;
  void OnUiElementsCleared() override;

 private:
  void InitLayout();

  void OnCardElementAdded(const AssistantCardElement* card_element);
  void OnCardReady(const base::Optional<base::UnguessableToken>& embed_token);
  void OnTextElementAdded(const AssistantTextElement* text_element);

  // Assistant cards are rendered asynchronously before being added to the view
  // hierarchy. For this reason, it is necessary to pend any UI elements that
  // arrive between the time a render request is sent and the time at which the
  // view is finally embedded. Failure to do so could result in a mismatch
  // between the ordering of UI elements received and their corresponding views.
  void SetProcessingUiElement(bool is_processing);
  void ProcessPendingUiElements();

  void ReleaseAllCards();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  std::unique_ptr<AssistantHeaderView> assistant_header_view_;

  // Uniquely identifies cards owned by AssistantCardRenderer.
  std::vector<base::UnguessableToken> id_token_list_;

  // Owned by AssistantInteractionModel.
  std::deque<const AssistantUiElement*> pending_ui_element_list_;

  // Whether a UI element is currently being processed. If true, new UI elements
  // are added to |pending_ui_element_list_| and processed later.
  bool is_processing_ui_element_ = false;

  // Weak pointer factory used for card rendering requests.
  base::WeakPtrFactory<UiElementContainerView> render_request_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiElementContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
