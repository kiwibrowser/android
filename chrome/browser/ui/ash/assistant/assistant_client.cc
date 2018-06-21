// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/assistant_client.h"

#include <utility>

#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "chrome/browser/chromeos/arc/voice_interaction/voice_interaction_controller_client.h"
#include "chrome/browser/ui/ash/assistant/assistant_context_util.h"
#include "chrome/browser/ui/ash/assistant/assistant_image_downloader.h"
#include "chrome/browser/ui/ash/assistant/assistant_setup.h"
#include "chrome/browser/ui/ash/assistant/web_contents_manager.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {
// Owned by ChromeBrowserMainChromeOS:
AssistantClient* g_instance = nullptr;
}  // namespace

// static
AssistantClient* AssistantClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

AssistantClient::AssistantClient() : client_binding_(this) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantClient::~AssistantClient() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

void AssistantClient::MaybeInit(service_manager::Connector* connector) {
  if (initialized_)
    return;

  initialized_ = true;
  connector->BindInterface(chromeos::assistant::mojom::kServiceName,
                           &assistant_connection_);

  chromeos::assistant::mojom::ClientPtr client_ptr;
  client_binding_.Bind(mojo::MakeRequest(&client_ptr));
  assistant_connection_->Init(std::move(client_ptr));

  assistant_image_downloader_ =
      std::make_unique<AssistantImageDownloader>(connector);
  web_contents_manager_ = std::make_unique<WebContentsManager>(connector);
  assistant_setup_ = std::make_unique<AssistantSetup>(connector);
}

void AssistantClient::OnAssistantStatusChanged(bool running) {
  arc::VoiceInteractionControllerClient::Get()->NotifyStatusChanged(
      running ? ash::mojom::VoiceInteractionState::RUNNING
              : ash::mojom::VoiceInteractionState::STOPPED);
}

void AssistantClient::RequestAssistantStructure(
    RequestAssistantStructureCallback callback) {
  RequestAssistantStructureForActiveBrowserWindow(std::move(callback));
}
