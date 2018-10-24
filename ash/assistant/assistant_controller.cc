// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_controller.h"

#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

AssistantController::AssistantController()
    : assistant_interaction_controller_(
          std::make_unique<AssistantInteractionController>()),
      assistant_ui_controller_(std::make_unique<AssistantUiController>(this)) {}

AssistantController::~AssistantController() = default;

void AssistantController::BindRequest(
    mojom::AssistantControllerRequest request) {
  assistant_controller_bindings_.AddBinding(this, std::move(request));
}

void AssistantController::SetAssistant(
    chromeos::assistant::mojom::AssistantPtr assistant) {
  assistant_ = std::move(assistant);

  // Provide reference to interaction controller.
  assistant_interaction_controller_->SetAssistant(assistant_.get());
}

void AssistantController::SetAssistantImageDownloader(
    mojom::AssistantImageDownloaderPtr assistant_image_downloader) {
  assistant_image_downloader_ = std::move(assistant_image_downloader);
}

void AssistantController::SetAssistantSetup(
    mojom::AssistantSetupPtr assistant_setup) {
  assistant_setup_ = std::move(assistant_setup);

  // Provide reference to interaction controller.
  assistant_interaction_controller_->SetAssistantSetup(assistant_setup_.get());
}

void AssistantController::SetWebContentsManager(
    mojom::WebContentsManagerPtr web_contents_manager) {
  web_contents_manager_ = std::move(web_contents_manager);
}

void AssistantController::RequestScreenshot(
    const gfx::Rect& rect,
    RequestScreenshotCallback callback) {
  // TODO(muyuanli): handle multi-display when assistant's behavior is defined.
  auto* root_window = Shell::GetPrimaryRootWindow();
  gfx::Rect source_rect =
      rect.IsEmpty() ? gfx::Rect(root_window->bounds().size()) : rect;
  ui::GrabWindowSnapshotAsyncJPEG(
      root_window, source_rect,
      base::BindRepeating(
          [](RequestScreenshotCallback callback,
             scoped_refptr<base::RefCountedMemory> data) {
            std::move(callback).Run(std::vector<uint8_t>(
                data->front(), data->front() + data->size()));
          },
          base::Passed(&callback)));
}

void AssistantController::ManageWebContents(
    const base::UnguessableToken& id_token,
    mojom::ManagedWebContentsParamsPtr params,
    mojom::WebContentsManager::ManageWebContentsCallback callback) {
  DCHECK(web_contents_manager_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(base::nullopt);
    return;
  }

  // Supply account ID.
  params->account_id = user_session->user_info->account_id;

  // Specify that we will handle top level browser requests.
  ash::mojom::ManagedWebContentsOpenUrlDelegatePtr ptr;
  web_contents_open_url_delegate_bindings_.AddBinding(
      assistant_interaction_controller_.get(), mojo::MakeRequest(&ptr));
  params->open_url_delegate_ptr_info = ptr.PassInterface();

  web_contents_manager_->ManageWebContents(id_token, std::move(params),
                                           std::move(callback));
}

void AssistantController::ReleaseWebContents(
    const base::UnguessableToken& id_token) {
  web_contents_manager_->ReleaseWebContents(id_token);
}

void AssistantController::ReleaseWebContents(
    const std::vector<base::UnguessableToken>& id_tokens) {
  web_contents_manager_->ReleaseAllWebContents(id_tokens);
}

void AssistantController::DownloadImage(
    const GURL& url,
    mojom::AssistantImageDownloader::DownloadCallback callback) {
  DCHECK(assistant_image_downloader_);

  const mojom::UserSession* user_session =
      Shell::Get()->session_controller()->GetUserSession(0);

  if (!user_session) {
    LOG(WARNING) << "Unable to retrieve active user session.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  AccountId account_id = user_session->user_info->account_id;
  assistant_image_downloader_->Download(account_id, url, std::move(callback));
}

// TODO(dmblack): Update DialogPlate to accept multiple listeners and then
// remove this code from AssistantController. Use observer pattern.
void AssistantController::OnDialogPlateButtonPressed(DialogPlateButtonId id) {
  assistant_interaction_controller_->OnDialogPlateButtonPressed(id);
  assistant_ui_controller_->OnDialogPlateButtonPressed(id);
}

// TODO(dmblack): Update DialogPlate to accept multiple listeners and then
// remove this code from AssistantController. Use observer pattern.
void AssistantController::OnDialogPlateContentsCommitted(
    const std::string& text) {
  assistant_interaction_controller_->OnDialogPlateContentsCommitted(text);
  assistant_ui_controller_->OnDialogPlateContentsCommitted(text);
}

}  // namespace ash
