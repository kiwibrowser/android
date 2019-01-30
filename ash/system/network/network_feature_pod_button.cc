// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_feature_pod_button.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::NetworkConnectionHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {

namespace {

bool IsActive() {
  return NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
             NetworkTypePattern::NonVirtual()) != nullptr;
}

const NetworkState* GetCurrentNetwork() {
  NetworkStateHandler* state_handler =
      NetworkHandler::Get()->network_state_handler();
  NetworkConnectionHandler* connect_handler =
      NetworkHandler::Get()->network_connection_handler();
  const NetworkState* connected_network =
      state_handler->ConnectedNetworkByType(NetworkTypePattern::NonVirtual());
  const NetworkState* connecting_network =
      state_handler->ConnectingNetworkByType(NetworkTypePattern::Wireless());
  // If we are connecting to a network, and there is either no connected
  // network, or the connection was user requested, or shill triggered a
  // reconnection, use the connecting network.
  if (connecting_network &&
      (!connected_network || connecting_network->IsReconnecting() ||
       connect_handler->HasConnectingNetwork(connecting_network->path()))) {
    return connecting_network;
  }

  if (connected_network)
    return connected_network;

  // If no connecting network, check if we are activating a network.
  const NetworkState* mobile_network =
      state_handler->FirstNetworkByType(NetworkTypePattern::Mobile());
  if (mobile_network && (mobile_network->activation_state() ==
                         shill::kActivationStateActivating)) {
    return mobile_network;
  }

  return nullptr;
}

}  // namespace

NetworkFeaturePodButton::NetworkFeaturePodButton(
    FeaturePodControllerBase* controller)
    : FeaturePodButton(controller) {
  // NetworkHandler can be uninitialized in unit tests.
  if (!NetworkHandler::IsInitialized())
    return;
  network_state_observer_ = std::make_unique<TrayNetworkStateObserver>(this);
  Update();
}

NetworkFeaturePodButton::~NetworkFeaturePodButton() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkFeaturePodButton::NetworkIconChanged() {
  Update();
}

void NetworkFeaturePodButton::NetworkStateChanged(bool notify_a11y) {
  Update();
}

void NetworkFeaturePodButton::Update() {
  gfx::ImageSkia image;
  bool animating = false;
  network_icon::GetDefaultNetworkImageAndLabel(
      network_icon::ICON_TYPE_DEFAULT_VIEW, &image, nullptr, &animating);

  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  SetToggled(
      IsActive() ||
      NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::WiFi()));
  icon_button()->SetImage(views::Button::STATE_NORMAL, image);

  const NetworkState* network = GetCurrentNetwork();
  if (!network) {
    SetLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_LABEL));
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_DISCONNECTED_SUBLABEL));
    return;
  }

  SetLabel(network->Matches(NetworkTypePattern::Ethernet())
               ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET)
               : base::UTF8ToUTF16(network->name()));

  if (network->IsReconnecting() || network->IsConnectingState()) {
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING_SUBLABEL));
    return;
  }

  if (network->IsConnectedState()) {
    switch (network_icon::GetSignalStrengthForNetwork(network)) {
      case network_icon::SignalStrength::WEAK:
        SetSubLabel(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_WEAK_SUBLABEL));
        break;
      case network_icon::SignalStrength::MEDIUM:
        SetSubLabel(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_MEDIUM_SUBLABEL));
        break;
      case network_icon::SignalStrength::STRONG:
        SetSubLabel(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_SIGNAL_STRONG_SUBLABEL));
        break;
      case network_icon::SignalStrength::NONE:
        FALLTHROUGH;
      case network_icon::SignalStrength::NOT_WIRELESS:
        SetSubLabel(l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED));
        break;
    }
    return;
  }

  if (network->activation_state() == shill::kActivationStateActivating) {
    SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_ACTIVATING_SUBLABEL));
    return;
  }
}

}  // namespace ash
