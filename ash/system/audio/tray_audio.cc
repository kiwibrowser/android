// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/tray_audio.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/audio/volume_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item_detailed_view_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/views/view.h"

namespace ash {

using chromeos::CrasAudioHandler;

TrayAudio::TrayAudio(SystemTray* system_tray)
    : TrayImageItem(system_tray,
                    kSystemTrayVolumeMuteIcon,
                    SystemTrayItemUmaType::UMA_AUDIO),
      volume_view_(nullptr),
      pop_up_volume_view_(false),
      audio_detail_view_(nullptr),
      detailed_view_delegate_(
          std::make_unique<SystemTrayItemDetailedViewDelegate>(this)) {
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->AddAudioObserver(this);
}

TrayAudio::~TrayAudio() {
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void TrayAudio::ShowPopUpVolumeView() {
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  float percent = CrasAudioHandler::Get()->GetOutputVolumePercent() / 100.0f;
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }

  pop_up_volume_view_ = true;
  ShowDetailedView(kTrayPopupAutoCloseDelayInSeconds);
}

bool TrayAudio::GetInitialVisibility() {
  return CrasAudioHandler::Get()->IsOutputMuted();
}

views::View* TrayAudio::CreateDefaultView(LoginStatus status) {
  volume_view_ = new tray::VolumeView(this, true);
  return volume_view_;
}

views::View* TrayAudio::CreateDetailedView(LoginStatus status) {
  if (pop_up_volume_view_) {
    volume_view_ = new tray::VolumeView(this, false);
    return volume_view_;
  } else {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_DETAILED_AUDIO_VIEW);
    audio_detail_view_ =
        new tray::AudioDetailedView(detailed_view_delegate_.get());
    return audio_detail_view_;
  }
}

void TrayAudio::OnDefaultViewDestroyed() {
  volume_view_ = nullptr;
}

void TrayAudio::OnDetailedViewDestroyed() {
  if (audio_detail_view_) {
    audio_detail_view_ = nullptr;
  } else if (volume_view_) {
    volume_view_ = nullptr;
    pop_up_volume_view_ = false;
  }
}

bool TrayAudio::ShouldShowShelf() const {
  return !pop_up_volume_view_;
}

views::View* TrayAudio::GetItemToRestoreFocusTo() {
  if (!volume_view_)
    return nullptr;

  // The more button on |volume_view_| is the view that triggered the detail
  // view, so it should grab focus when going back to the default view.
  return volume_view_->more_button();
}

void TrayAudio::OnOutputNodeVolumeChanged(uint64_t /* node_id */,
                                          int /* volume */) {
  ShowPopUpVolumeView();
}

void TrayAudio::OnOutputMuteChanged(bool /* mute_on */, bool system_adjust) {
  if (features::IsSystemTrayUnifiedEnabled())
    return;

  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->Update();
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
  } else if (!system_adjust) {
    pop_up_volume_view_ = true;
    ShowDetailedView(kTrayPopupAutoCloseDelayInSeconds);
  }
}

void TrayAudio::OnAudioNodesChanged() {
  Update();
}

void TrayAudio::OnActiveOutputNodeChanged() {
  Update();
}

void TrayAudio::OnActiveInputNodeChanged() {
  Update();
}

void TrayAudio::Update() {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());
  if (volume_view_) {
    volume_view_->SetVolumeLevel(
        CrasAudioHandler::Get()->GetOutputVolumePercent() / 100.0f);
    volume_view_->Update();
  }

  if (audio_detail_view_)
    audio_detail_view_->Update();
}

}  // namespace ash
