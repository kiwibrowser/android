// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/mojo_version_info_dispatcher.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

MojoVersionInfoDispatcher::MojoVersionInfoDispatcher()
    : version_info_updater_(this) {}

MojoVersionInfoDispatcher::~MojoVersionInfoDispatcher() {}

void MojoVersionInfoDispatcher::StartUpdate() {
  version_info::Channel channel = chrome::GetChannel();
  bool should_show_version = channel != version_info::Channel::STABLE &&
                             channel != version_info::Channel::BETA;
  if (should_show_version) {
#if defined(OFFICIAL_BUILD)
    version_info_updater_.StartUpdate(true);
#else
    version_info_updater_.StartUpdate(false);
#endif
  }
}

void MojoVersionInfoDispatcher::OnOSVersionLabelTextUpdated(
    const std::string& os_version_label_text) {
  os_version_label_text_ = os_version_label_text;
  OnDevChannelInfoUpdated();
}

void MojoVersionInfoDispatcher::OnEnterpriseInfoUpdated(
    const std::string& message_text,
    const std::string& asset_id) {
  if (asset_id.empty())
    return;
  enterprise_info_text_ = l10n_util::GetStringFUTF8(
      IDS_OOBE_ASSET_ID_LABEL, base::UTF8ToUTF16(asset_id));
  OnDevChannelInfoUpdated();
}

void MojoVersionInfoDispatcher::OnDeviceInfoUpdated(
    const std::string& bluetooth_name) {
  bluetooth_name_ = bluetooth_name;
  OnDevChannelInfoUpdated();
}

void MojoVersionInfoDispatcher::OnDevChannelInfoUpdated() {
  LoginScreenClient::Get()->login_screen()->SetDevChannelInfo(
      os_version_label_text_, enterprise_info_text_, bluetooth_name_);
}

}  // namespace chromeos
