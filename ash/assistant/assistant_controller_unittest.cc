// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include <memory>

#include "ash/assistant/assistant_ui_controller.h"
#include "ash/highlighter/highlighter_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/services/assistant/test_support/mock_assistant.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class AssistantControllerTest : public AshTestBase {
 protected:
  AssistantControllerTest() = default;
  ~AssistantControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::switches::kAssistantFeature);
    ASSERT_TRUE(chromeos::switches::IsAssistantEnabled());

    AshTestBase::SetUp();

    controller_ = Shell::Get()->assistant_controller();
    DCHECK(controller_);

    // Mock Assistant.
    assistant_ = std::make_unique<chromeos::assistant::MockAssistant>();
    assistant_binding_ =
        std::make_unique<mojo::Binding<chromeos::assistant::mojom::Assistant>>(
            assistant_.get());
    chromeos::assistant::mojom::AssistantPtr assistant;
    assistant_binding_->Bind(mojo::MakeRequest(&assistant));
    controller_->SetAssistant(std::move(assistant));

    ASSERT_FALSE(IsAssistantUiShown());
  }

  bool IsAssistantUiShown() {
    return controller_->ui_controller()->IsVisible();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  AssistantController* controller_ = nullptr;

  std::unique_ptr<chromeos::assistant::MockAssistant> assistant_;

  std::unique_ptr<mojo::Binding<chromeos::assistant::mojom::Assistant>>
      assistant_binding_;

  DISALLOW_COPY_AND_ASSIGN(AssistantControllerTest);
};

}  // namespace ash
