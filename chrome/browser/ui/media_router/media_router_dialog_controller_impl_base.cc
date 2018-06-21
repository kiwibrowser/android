// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_dialog_controller_impl_base.h"

#include <utility>

#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/media_router_ui_base.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui_service.h"

using content::WebContents;

namespace media_router {

namespace {

MediaRouterUIService* GetMediaRouterUIService(WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  // TODO(crbug.com/826091): Move MRUIService to c/b/ui/media_router/.
  return MediaRouterUIService::Get(profile);
}

}  // namespace

MediaRouterDialogControllerImplBase::~MediaRouterDialogControllerImplBase() {
  media_router_ui_service_->RemoveObserver(this);
}

void MediaRouterDialogControllerImplBase::SetMediaRouterAction(
    const base::WeakPtr<MediaRouterAction>& action) {
  action_ = action;
}

void MediaRouterDialogControllerImplBase::CreateMediaRouterDialog() {
  if (!GetActionController())
    return;

  // The |GetActionController_| must be notified after |action_| to avoid a UI
  // bug in which the drop shadow is drawn in an incorrect position.
  if (action_)
    action_->OnDialogShown();
  GetActionController()->OnDialogShown();
}

void MediaRouterDialogControllerImplBase::Reset() {
  if (IsShowingMediaRouterDialog()) {
    if (action_)
      action_->OnDialogHidden();
    if (GetActionController())
      GetActionController()->OnDialogHidden();
  }
  MediaRouterDialogController::Reset();
}

MediaRouterDialogControllerImplBase::MediaRouterDialogControllerImplBase(
    WebContents* web_contents)
    : MediaRouterDialogController(web_contents),
      media_router_ui_service_(GetMediaRouterUIService(web_contents)) {
  DCHECK(media_router_ui_service_);
  media_router_ui_service_->AddObserver(this);
}

void MediaRouterDialogControllerImplBase::InitializeMediaRouterUI(
    MediaRouterUIBase* media_router_ui) {
  auto start_presentation_context = std::move(start_presentation_context_);
  PresentationServiceDelegateImpl* delegate =
      PresentationServiceDelegateImpl::FromWebContents(initiator());
  if (!start_presentation_context) {
    media_router_ui->InitWithDefaultMediaSource(initiator(), delegate);
  } else {
    media_router_ui->InitWithStartPresentationContext(
        initiator(), delegate, std::move(start_presentation_context));
  }
}

void MediaRouterDialogControllerImplBase::OnServiceDisabled() {
  CloseMediaRouterDialog();
  Reset();
}

MediaRouterActionController*
MediaRouterDialogControllerImplBase::GetActionController() {
  return media_router_ui_service_->action_controller();
}

}  // namespace media_router
