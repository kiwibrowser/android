// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_tray_controller.h"

#include "ash/metrics/user_metrics_action.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/audio/unified_audio_detailed_view_controller.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/bluetooth/bluetooth_feature_pod_controller.h"
#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/cast/cast_feature_pod_controller.h"
#include "ash/system/cast/unified_cast_detailed_view_controller.h"
#include "ash/system/ime/ime_feature_pod_controller.h"
#include "ash/system/ime/unified_ime_detailed_view_controller.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_feature_pod_controller.h"
#include "ash/system/network/unified_network_detailed_view_controller.h"
#include "ash/system/network/unified_vpn_detailed_view_controller.h"
#include "ash/system/network/vpn_feature_pod_controller.h"
#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/system/rotation/rotation_lock_feature_pod_controller.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/system_tray_item_uma_type.h"
#include "ash/system/unified/accessibility_feature_pod_controller.h"
#include "ash/system/unified/detailed_view_controller.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_pod_controller_base.h"
#include "ash/system/unified/quiet_mode_feature_pod_controller.h"
#include "ash/system/unified/unified_notifier_settings_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/unified/unified_system_tray_view.h"
#include "ash/system/unified/user_chooser_view.h"
#include "ash/system/unified_accessibility_detailed_view_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/message_center/message_center.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Animation duration to collapse / expand the view in milliseconds.
const int kExpandAnimationDurationMs = 500;
// Threshold in pixel that fully collapses / expands the view through gesture.
const int kDragThreshold = 200;

}  // namespace

UnifiedSystemTrayController::UnifiedSystemTrayController(
    UnifiedSystemTrayModel* model)
    : model_(model), animation_(std::make_unique<gfx::SlideAnimation>(this)) {
  animation_->Reset(model->expanded_on_open() ? 1.0 : 0.0);
  animation_->SetSlideDuration(kExpandAnimationDurationMs);
  animation_->SetTweenType(gfx::Tween::EASE_IN_OUT);
}

UnifiedSystemTrayController::~UnifiedSystemTrayController() = default;

UnifiedSystemTrayView* UnifiedSystemTrayController::CreateView() {
  DCHECK(!unified_view_);
  unified_view_ = new UnifiedSystemTrayView(this, model_->expanded_on_open());
  InitFeaturePods();

  volume_slider_controller_ =
      std::make_unique<UnifiedVolumeSliderController>(this);
  unified_view_->AddSliderView(volume_slider_controller_->CreateView());

  brightness_slider_controller_ =
      std::make_unique<UnifiedBrightnessSliderController>(model_);
  unified_view_->AddSliderView(brightness_slider_controller_->CreateView());

  return unified_view_;
}

void UnifiedSystemTrayController::HandleUserSwitch(int user_index) {
  // Do not switch users when the log screen is presented.
  SessionController* controller = Shell::Get()->session_controller();
  if (controller->IsUserSessionBlocked())
    return;

  // |user_index| must be in range (0, number_of_user). Note 0 is excluded
  // because it represents the active user and SwitchUser should not be called
  // for such case.
  DCHECK_GT(user_index, 0);
  DCHECK_LT(user_index, controller->NumberOfLoggedInUsers());

  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  controller->SwitchActiveUser(
      controller->GetUserSession(user_index)->user_info->account_id);
}

void UnifiedSystemTrayController::HandleAddUserAction() {
  MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
  Shell::Get()->session_controller()->ShowMultiProfileLogin();
}

void UnifiedSystemTrayController::HandleSignOutAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_STATUS_AREA_SIGN_OUT);
  Shell::Get()->session_controller()->RequestSignOut();
}

void UnifiedSystemTrayController::HandleLockAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_LOCK_SCREEN);
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();
}

void UnifiedSystemTrayController::HandleSettingsAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_SETTINGS);
  Shell::Get()->system_tray_controller()->ShowSettings();
  CloseBubble();
}

void UnifiedSystemTrayController::HandlePowerAction() {
  Shell::Get()->metrics()->RecordUserMetricsAction(UMA_TRAY_SHUT_DOWN);
  Shell::Get()->lock_state_controller()->RequestShutdown(
      ShutdownReason::TRAY_SHUT_DOWN_BUTTON);
}

void UnifiedSystemTrayController::HandleOpenDateTimeSettingsAction() {
  if (Shell::Get()->system_tray_model()->clock()->can_set_time()) {
    Shell::Get()->system_tray_model()->clock()->ShowSetTimeDialog();
  } else {
    Shell::Get()->system_tray_model()->clock()->ShowDateSettings();
  }
  CloseBubble();
}

void UnifiedSystemTrayController::ToggleExpanded() {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.ToggleExpanded",
                            TOGGLE_EXPANDED_TYPE_BY_BUTTON,
                            TOGGLE_EXPANDED_TYPE_COUNT);
  if (animation_->IsShowing())
    animation_->Hide();
  else
    animation_->Show();
}

void UnifiedSystemTrayController::HandleClearAllAction() {
  // When the animation is finished, OnClearAllAnimationEnded() is called.
  unified_view_->ShowClearAllAnimation();
}

void UnifiedSystemTrayController::OnClearAllAnimationEnded() {
  message_center::MessageCenter::Get()->RemoveAllNotifications(
      true /* by_user */,
      message_center::MessageCenter::RemoveType::NON_PINNED);
}

void UnifiedSystemTrayController::BeginDrag(const gfx::Point& location) {
  drag_init_point_ = location;
  was_expanded_ = animation_->IsShowing();
}

void UnifiedSystemTrayController::UpdateDrag(const gfx::Point& location) {
  animation_->Reset(GetDragExpandedAmount(location));
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::EndDrag(const gfx::Point& location) {
  bool expanded = GetDragExpandedAmount(location) > 0.5;
  if (was_expanded_ != expanded) {
    UMA_HISTOGRAM_ENUMERATION("ChromeOS.SystemTray.ToggleExpanded",
                              TOGGLE_EXPANDED_TYPE_BY_GESTURE,
                              TOGGLE_EXPANDED_TYPE_COUNT);
  }

  // If dragging is finished, animate to closer state.
  if (expanded) {
    animation_->Show();
  } else {
    // To animate to hidden state, first set SlideAnimation::IsShowing() to
    // true.
    animation_->Show();
    animation_->Hide();
  }
}

void UnifiedSystemTrayController::ShowUserChooserWidget() {
  // Don't allow user add or switch when CancelCastingDialog is open.
  // See http://crrev.com/291276 and http://crbug.com/353170.
  if (Shell::IsSystemModalWindowOpen())
    return;

  // Don't allow at login, lock or when adding a multi-profile user.
  SessionController* session = Shell::Get()->session_controller();
  if (session->IsUserSessionBlocked())
    return;

  // Don't show if we cannot add or switch users.
  if (session->GetAddUserPolicy() != AddUserSessionPolicy::ALLOWED &&
      session->NumberOfLoggedInUsers() <= 1)
    return;

  unified_view_->SetDetailedView(new UserChooserView(this));
}

void UnifiedSystemTrayController::ShowNetworkDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_NETWORK_VIEW);
  ShowDetailedView(
      std::make_unique<UnifiedNetworkDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowBluetoothDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW);
  ShowDetailedView(
      std::make_unique<UnifiedBluetoothDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowCastDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_CAST_VIEW);
  ShowDetailedView(std::make_unique<UnifiedCastDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowAccessibilityDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_ACCESSIBILITY);
  ShowDetailedView(
      std::make_unique<UnifiedAccessibilityDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowVPNDetailedView() {
  Shell::Get()->metrics()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DETAILED_VPN_VIEW);
  ShowDetailedView(std::make_unique<UnifiedVPNDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowIMEDetailedView() {
  ShowDetailedView(std::make_unique<UnifiedIMEDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowAudioDetailedView() {
  ShowDetailedView(std::make_unique<UnifiedAudioDetailedViewController>(this));
}

void UnifiedSystemTrayController::ShowNotifierSettingsView() {
  ShowDetailedView(std::make_unique<UnifiedNotifierSettingsController>(this));
}

void UnifiedSystemTrayController::TransitionToMainView(bool restore_focus) {
  detailed_view_controller_.reset();
  unified_view_->ResetDetailedView();
  if (restore_focus)
    unified_view_->RestoreFeaturePodFocus();
}

void UnifiedSystemTrayController::CloseBubble() {
  if (unified_view_->GetWidget())
    unified_view_->GetWidget()->Close();
}

void UnifiedSystemTrayController::AnimationEnded(
    const gfx::Animation* animation) {
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationProgressed(
    const gfx::Animation* animation) {
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::AnimationCanceled(
    const gfx::Animation* animation) {
  animation_->Reset(std::round(animation_->GetCurrentValue()));
  UpdateExpandedAmount();
}

void UnifiedSystemTrayController::InitFeaturePods() {
  AddFeaturePodItem(std::make_unique<NetworkFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<BluetoothFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<QuietModeFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<RotationLockFeaturePodController>());
  AddFeaturePodItem(std::make_unique<NightLightFeaturePodController>());
  AddFeaturePodItem(std::make_unique<CastFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<AccessibilityFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<VPNFeaturePodController>(this));
  AddFeaturePodItem(std::make_unique<IMEFeaturePodController>(this));

  // If you want to add a new feature pod item, add here.
}

void UnifiedSystemTrayController::AddFeaturePodItem(
    std::unique_ptr<FeaturePodControllerBase> controller) {
  DCHECK(unified_view_);
  FeaturePodButton* button = controller->CreateButton();

  // Record DefaultView.VisibleRows UMA.
  SystemTrayItemUmaType uma_type = controller->GetUmaType();
  if (uma_type != SystemTrayItemUmaType::UMA_NOT_RECORDED &&
      button->visible_preferred()) {
    UMA_HISTOGRAM_ENUMERATION("Ash.SystemMenu.DefaultView.VisibleRows",
                              uma_type, SystemTrayItemUmaType::UMA_COUNT);
  }

  unified_view_->AddFeaturePodButton(button);
  feature_pod_controllers_.push_back(std::move(controller));
}

void UnifiedSystemTrayController::ShowDetailedView(
    std::unique_ptr<DetailedViewController> controller) {
  unified_view_->SetDetailedView(controller->CreateView());
  unified_view_->SaveFeaturePodFocus();
  detailed_view_controller_ = std::move(controller);
}

void UnifiedSystemTrayController::UpdateExpandedAmount() {
  double expanded_amount = animation_->GetCurrentValue();
  unified_view_->SetExpandedAmount(expanded_amount);
  if (expanded_amount == 0.0 || expanded_amount == 1.0)
    model_->set_expanded_on_open(expanded_amount == 1.0);
}

double UnifiedSystemTrayController::GetDragExpandedAmount(
    const gfx::Point& location) const {
  double y_diff = (location - drag_init_point_).y();

  // If already expanded, only consider swiping down. Otherwise, only consider
  // swiping up.
  if (was_expanded_) {
    return base::ClampToRange(1.0 - std::max(0.0, y_diff) / kDragThreshold, 0.0,
                              1.0);
  } else {
    return base::ClampToRange(std::max(0.0, -y_diff) / kDragThreshold, 0.0,
                              1.0);
  }
}

}  // namespace ash
