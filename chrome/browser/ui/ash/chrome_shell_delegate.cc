// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_shell_delegate.h"

#include <stddef.h>

#include <limits>
#include <vector>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/public/cpp/accessibility_types.h"
#include "ash/screenshot_delegate.h"
#include "base/macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/chrome_keyboard_ui.h"
#include "chrome/browser/ui/ash/chrome_screenshot_grabber.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/network/networking_config_delegate_chromeos.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/session_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/url_constants.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "url/url_constants.h"

using chromeos::AccessibilityManager;

namespace {

const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

class AccessibilityDelegateImpl : public ash::AccessibilityDelegate {
 public:
  AccessibilityDelegateImpl() = default;
  ~AccessibilityDelegateImpl() override = default;

  void SetMagnifierEnabled(bool enabled) override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->SetMagnifierEnabled(enabled);
  }

  bool IsMagnifierEnabled() const override {
    DCHECK(chromeos::MagnificationManager::Get());
    return chromeos::MagnificationManager::Get()->IsMagnifierEnabled();
  }

  bool ShouldShowAccessibilityMenu() const override {
    DCHECK(AccessibilityManager::Get());
    return AccessibilityManager::Get()->ShouldShowAccessibilityMenu();
  }

  void SaveScreenMagnifierScale(double scale) override {
    if (chromeos::MagnificationManager::Get())
      chromeos::MagnificationManager::Get()->SaveScreenMagnifierScale(scale);
  }

  double GetSavedScreenMagnifierScale() override {
    if (chromeos::MagnificationManager::Get()) {
      return chromeos::MagnificationManager::Get()
          ->GetSavedScreenMagnifierScale();
    }
    return std::numeric_limits<double>::min();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityDelegateImpl);
};

}  // namespace

ChromeShellDelegate::ChromeShellDelegate()
    : networking_config_delegate_(
          std::make_unique<chromeos::NetworkingConfigDelegateChromeos>()) {
}

ChromeShellDelegate::~ChromeShellDelegate() {}

service_manager::Connector* ChromeShellDelegate::GetShellConnector() const {
  return content::ServiceManagerConnection::GetForProcess()->GetConnector();
}

bool ChromeShellDelegate::CanShowWindowForUser(aura::Window* window) const {
  return ::CanShowWindowForUser(window, base::Bind(&GetActiveBrowserContext));
}

void ChromeShellDelegate::PreInit() {
  // TODO: port to mash. http://crbug.com/678949.
  if (!features::IsAshInBrowserProcess())
    return;

  // Object owns itself and deletes itself in OnWindowTreeHostManagerShutdown().
  // Setup is done in OnShellInitialized() so this needs to be constructed after
  // Shell is constructed but before OnShellInitialized() is called. Depends on
  // CroSettings. TODO(stevenjb): Move to src/ash.
  new policy::DisplayRotationDefaultHandler();
}

void ChromeShellDelegate::OpenKeyboardShortcutHelpPage() const {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  Browser* browser = chrome::FindTabbedBrowser(profile, false);

  if (!browser) {
    browser = new Browser(Browser::CreateParams(profile, true));
    browser->window()->Show();
  }

  browser->window()->Activate();

  NavigateParams params(browser, GURL(kKeyboardShortcutHelpPageUrl),
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  Navigate(&params);
}

std::unique_ptr<keyboard::KeyboardUI> ChromeShellDelegate::CreateKeyboardUI() {
  return std::make_unique<ChromeKeyboardUI>(
      ProfileManager::GetActiveUserProfile());
}

ash::AccessibilityDelegate* ChromeShellDelegate::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateImpl;
}

ash::NetworkingConfigDelegate*
ChromeShellDelegate::GetNetworkingConfigDelegate() {
  return networking_config_delegate_.get();
}

std::unique_ptr<ash::ScreenshotDelegate>
ChromeShellDelegate::CreateScreenshotDelegate() {
  return std::make_unique<ChromeScreenshotGrabber>();
}

ui::InputDeviceControllerClient*
ChromeShellDelegate::GetInputDeviceControllerClient() {
  return g_browser_process->platform_part()->GetInputDeviceControllerClient();
}
