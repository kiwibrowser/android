// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/screens/welcome_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/gfx/geometry/point.h"

namespace base {
class ListValue;
}

namespace chromeos {

class CoreOobeView;

// WebUI implementation of WelcomeScreenView. It is used to interact with
// the welcome screen (part of the page) of the OOBE.
class WelcomeScreenHandler : public WelcomeView, public BaseScreenHandler {
 public:
  explicit WelcomeScreenHandler(CoreOobeView* core_oobe_view);
  ~WelcomeScreenHandler() override;

 private:
  // WelcomeView implementation:
  void Show() override;
  void Hide() override;
  void Bind(WelcomeScreen* screen) override;
  void Unbind() override;
  void ShowError(const base::string16& message) override;
  void ClearErrors() override;
  void StopDemoModeDetection() override;
  void ShowConnectingStatus(bool connecting,
                            const base::string16& network_id) override;
  void ReloadLocalizedContent() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void Initialize() override;

  // Returns available timezones. Caller gets the ownership.
  static std::unique_ptr<base::ListValue> GetTimezoneList();

  CoreOobeView* core_oobe_view_ = nullptr;
  WelcomeScreen* screen_ = nullptr;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_ = false;

  // Position of the network control.
  gfx::Point network_control_pos_;

  DISALLOW_COPY_AND_ASSIGN(WelcomeScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_WELCOME_SCREEN_HANDLER_H_
