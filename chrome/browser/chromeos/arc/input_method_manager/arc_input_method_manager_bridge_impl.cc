// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"

#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_features.h"

namespace arc {

ArcInputMethodManagerBridgeImpl::ArcInputMethodManagerBridgeImpl(
    Delegate* delegate,
    ArcBridgeService* bridge_service)
    : delegate_(delegate), bridge_service_(bridge_service) {
  bridge_service_->input_method_manager()->SetHost(this);
}

ArcInputMethodManagerBridgeImpl::~ArcInputMethodManagerBridgeImpl() {
  bridge_service_->input_method_manager()->SetHost(nullptr);
}

void ArcInputMethodManagerBridgeImpl::SendEnableIme(
    const std::string& ime_id,
    bool enable,
    EnableImeCallback callback) {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), EnableIme);
  if (!imm_instance)
    return;

  if (!base::FeatureList::IsEnabled(kEnableInputMethodFeature))
    return;

  imm_instance->EnableIme(ime_id, enable, std::move(callback));
}

void ArcInputMethodManagerBridgeImpl::SendSwitchImeTo(
    const std::string& ime_id,
    SwitchImeToCallback callback) {
  auto* imm_instance = ARC_GET_INSTANCE_FOR_METHOD(
      bridge_service_->input_method_manager(), SwitchImeTo);
  if (!imm_instance)
    return;

  if (!base::FeatureList::IsEnabled(kEnableInputMethodFeature))
    return;

  imm_instance->SwitchImeTo(ime_id, std::move(callback));
}

void ArcInputMethodManagerBridgeImpl::OnActiveImeChanged(
    const std::string& ime_id) {
  delegate_->OnActiveImeChanged(ime_id);
}

void ArcInputMethodManagerBridgeImpl::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
  delegate_->OnImeInfoChanged(std::move(ime_info_array));
}

}  // namespace arc
