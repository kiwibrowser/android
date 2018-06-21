// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOJO_VERSION_INFO_DISPATCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOJO_VERSION_INFO_DISPATCHER_H_

#include "chrome/browser/chromeos/login/version_info_updater.h"

namespace chromeos {

// Used by both views based lock and login screen to observe the version info
// changes and make a mojo call to update the UI.
class MojoVersionInfoDispatcher : public VersionInfoUpdater::Delegate {
 public:
  MojoVersionInfoDispatcher();
  ~MojoVersionInfoDispatcher() override;

  // Start to request the version info.
  void StartUpdate();

  // VersionInfoUpdater::Delegate:
  void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) override;
  void OnEnterpriseInfoUpdated(const std::string& message_text,
                               const std::string& asset_id) override;
  void OnDeviceInfoUpdated(const std::string& bluetooth_name) override;

 private:
  // Invoked when version info changes to update UI.
  void OnDevChannelInfoUpdated();

  // Updates when version info is changed.
  VersionInfoUpdater version_info_updater_;

  std::string os_version_label_text_;
  std::string enterprise_info_text_;
  std::string bluetooth_name_;

  DISALLOW_COPY_AND_ASSIGN(MojoVersionInfoDispatcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOJO_VERSION_INFO_DISPATCHER_H_
