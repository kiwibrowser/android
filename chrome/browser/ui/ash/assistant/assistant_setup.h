// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_

#include "ash/public/interfaces/assistant_setup.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

// AssistantSetup is the class responsible for start Assistant OptIn flow.
class AssistantSetup : public ash::mojom::AssistantSetup {
 public:
  explicit AssistantSetup(service_manager::Connector* connector);
  ~AssistantSetup() override;

  // ash::mojom::AssistantSetup:
  void StartAssistantOptInFlow() override;

 private:
  mojo::Binding<ash::mojom::AssistantSetup> binding_;

  DISALLOW_COPY_AND_ASSIGN(AssistantSetup);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_SETUP_H_
