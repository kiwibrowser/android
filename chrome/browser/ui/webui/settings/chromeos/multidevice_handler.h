// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace chromeos {

namespace settings {

// Chrome "Multidevice" (a.k.a. "Connected Devices") settings page UI handler.
class MultideviceHandler : public ::settings::SettingsPageUIHandler {
 public:
  MultideviceHandler();
  ~MultideviceHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ::settings::SettingsPageUIHandler:
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleShowMultiDeviceSetupDialog(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(MultideviceHandler);
};

}  // namespace settings

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_MULTIDEVICE_HANDLER_H_
