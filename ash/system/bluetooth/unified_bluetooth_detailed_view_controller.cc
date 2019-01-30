// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/unified_bluetooth_detailed_view_controller.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/bluetooth/bluetooth_detailed_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/unified/unified_detailed_view_delegate.h"

namespace ash {

UnifiedBluetoothDetailedViewController::UnifiedBluetoothDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<UnifiedDetailedViewDelegate>(tray_controller)) {
  Shell::Get()->system_tray_notifier()->AddBluetoothObserver(this);
}

UnifiedBluetoothDetailedViewController::
    ~UnifiedBluetoothDetailedViewController() {
  Shell::Get()->system_tray_notifier()->RemoveBluetoothObserver(this);
}

views::View* UnifiedBluetoothDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::BluetoothDetailedView(
      detailed_view_delegate_.get(),
      Shell::Get()->session_controller()->login_status());
  view_->Update();
  return view_;
}

void UnifiedBluetoothDetailedViewController::OnBluetoothRefresh() {
  view_->Update();
}

void UnifiedBluetoothDetailedViewController::OnBluetoothDiscoveringChanged() {
  view_->Update();
}

}  // namespace ash
