// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_setup.h"

#include "ash/public/interfaces/assistant_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"
#include "services/service_manager/public/cpp/connector.h"

AssistantSetup::AssistantSetup(service_manager::Connector* connector)
    : binding_(this) {
  // Bind to the Assistant controller in ash.
  ash::mojom::AssistantControllerPtr assistant_controller;
  connector->BindInterface(ash::mojom::kServiceName, &assistant_controller);
  ash::mojom::AssistantSetupPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  assistant_controller->SetAssistantSetup(std::move(ptr));
}

AssistantSetup::~AssistantSetup() = default;

void AssistantSetup::StartAssistantOptInFlow() {
  if (chromeos::AssistantOptInDialog::IsActive())
    return;
  chromeos::AssistantOptInDialog::Show();
}
