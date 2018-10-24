// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

class AssistantImageDownloader;
class WebContentsManager;
class AssistantSetup;

// Class to handle all assistant in-browser-process functionalities.
class AssistantClient : chromeos::assistant::mojom::Client {
 public:
  static AssistantClient* Get();

  AssistantClient();
  ~AssistantClient() override;

  void MaybeInit(service_manager::Connector* connector);

  // assistant::mojom::Client overrides:
  void OnAssistantStatusChanged(bool running) override;
  void RequestAssistantStructure(
      RequestAssistantStructureCallback callback) override;

 private:
  mojo::Binding<chromeos::assistant::mojom::Client> client_binding_;
  chromeos::assistant::mojom::AssistantPlatformPtr assistant_connection_;

  std::unique_ptr<AssistantImageDownloader> assistant_image_downloader_;
  std::unique_ptr<AssistantSetup> assistant_setup_;

  std::unique_ptr<WebContentsManager> web_contents_manager_;

  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(AssistantClient);
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_ASSISTANT_CLIENT_H_
