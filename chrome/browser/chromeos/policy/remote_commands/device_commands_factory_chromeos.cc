// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_commands_factory_chromeos.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_fetch_status_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_reboot_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_screenshot_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/device_command_set_volume_job.h"
#include "chrome/browser/chromeos/policy/remote_commands/screenshot_delegate.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

DeviceCommandsFactoryChromeOS::DeviceCommandsFactoryChromeOS() = default;

DeviceCommandsFactoryChromeOS::~DeviceCommandsFactoryChromeOS() = default;

std::unique_ptr<RemoteCommandJob>
DeviceCommandsFactoryChromeOS::BuildJobForType(em::RemoteCommand_Type type) {
  switch (type) {
    case em::RemoteCommand_Type_DEVICE_REBOOT:
      return std::make_unique<DeviceCommandRebootJob>(
          chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
    case em::RemoteCommand_Type_DEVICE_SCREENSHOT:
      return std::make_unique<DeviceCommandScreenshotJob>(
          std::make_unique<ScreenshotDelegate>());
    case em::RemoteCommand_Type_DEVICE_SET_VOLUME:
      return std::make_unique<DeviceCommandSetVolumeJob>();
    case em::RemoteCommand_Type_DEVICE_FETCH_STATUS:
      return std::make_unique<DeviceCommandFetchStatusJob>();
    default:
      // Other types of commands should be sent to UserCommandsFactoryChromeOS
      // instead of here.
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace policy
