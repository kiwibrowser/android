// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace settings {

MultideviceHandler::MultideviceHandler() = default;
MultideviceHandler::~MultideviceHandler() = default;

void MultideviceHandler::OnJavascriptAllowed() {}
void MultideviceHandler::OnJavascriptDisallowed() {}

void MultideviceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "showMultiDeviceSetupDialog",
      base::BindRepeating(&MultideviceHandler::HandleShowMultiDeviceSetupDialog,
                          base::Unretained(this)));
}

void MultideviceHandler::HandleShowMultiDeviceSetupDialog(
    const base::ListValue* args) {
  DCHECK(args->empty());
  multidevice_setup::MultiDeviceSetupDialog::Show();
}

}  // namespace settings

}  // namespace chromeos
