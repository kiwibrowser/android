// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_mojo.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

LoginDisplayMojo::LoginDisplayMojo(LoginDisplayHostMojo* host) : host_(host) {
  user_manager::UserManager::Get()->AddObserver(this);
}

LoginDisplayMojo::~LoginDisplayMojo() {
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void LoginDisplayMojo::ClearAndEnablePassword() {}

void LoginDisplayMojo::Init(const user_manager::UserList& filtered_users,
                            bool show_guest,
                            bool show_users,
                            bool show_new_user) {
  host_->SetUsers(filtered_users);

  // Load the login screen.
  auto* client = LoginScreenClient::Get();
  client->SetDelegate(host_);
  client->login_screen()->ShowLoginScreen(base::BindOnce([](bool did_show) {
    CHECK(did_show);

    // Some auto-tests depend on login-prompt-visible, like
    // login_SameSessionTwice.
    VLOG(1) << "Emitting login-prompt-visible";
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->EmitLoginPromptVisible();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());
  }));

  UserSelectionScreen* user_selection_screen = host_->user_selection_screen();
  user_selection_screen->Init(filtered_users);
  client->login_screen()->LoadUsers(
      user_selection_screen->UpdateAndReturnUserListForMojo(), show_guest);
  user_selection_screen->SetUsersLoaded(true /*loaded*/);
}

void LoginDisplayMojo::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::SetUIEnabled(bool is_enabled) {
  if (is_enabled)
    host_->GetOobeUI()->ShowOobeUI(false);
}

void LoginDisplayMojo::ShowError(int error_msg_id,
                                 int login_attempts,
                                 HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(jdufault): Investigate removing this method once views-based
  // login is fully implemented. Tracking bug at http://crbug/851680.
  VLOG(1) << "Show error, error_id: " << error_msg_id
          << ", attempts:" << login_attempts
          << ", help_topic_id: " << help_topic_id;
  if (!webui_handler_)
    return;

  std::string error_text;
  switch (error_msg_id) {
    case IDS_LOGIN_ERROR_CAPTIVE_PORTAL:
      error_text = l10n_util::GetStringFUTF8(
          error_msg_id, delegate()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(error_msg_id);
      break;
  }

  // Only display hints about keyboard layout if the error is authentication-
  // related.
  if (error_msg_id != IDS_LOGIN_ERROR_WHITELIST &&
      error_msg_id != IDS_ENTERPRISE_LOGIN_ERROR_WHITELIST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_KEY_LOST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_REQUIRED &&
      error_msg_id != IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED) {
    // Display a warning if Caps Lock is on.
    input_method::InputMethodManager* ime_manager =
        input_method::InputMethodManager::Get();
    if (ime_manager->GetImeKeyboard()->CapsLockIsEnabled()) {
      // TODO(ivankr): use a format string instead of concatenation.
      error_text +=
          "\n" + l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_CAPS_LOCK_HINT);
    }

    // Display a hint to switch keyboards if there are other active input
    // methods.
    if (ime_manager->GetActiveIMEState()->GetNumActiveInputMethods() > 1) {
      error_text +=
          "\n" + l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
    }
  }

  std::string help_link;
  if (login_attempts > 1)
    help_link = l10n_util::GetStringUTF8(IDS_LEARN_MORE);

  webui_handler_->ShowError(login_attempts, error_text, help_link,
                            help_topic_id);
}

void LoginDisplayMojo::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  host_->ShowErrorScreen(error_id);
}

void LoginDisplayMojo::ShowPasswordChangedDialog(bool show_password_error,
                                                 const std::string& email) {
  host_->ShowPasswordChangedDialog(show_password_error, email);
}

void LoginDisplayMojo::ShowSigninUI(const std::string& email) {
  host_->ShowSigninUI(email);
}

void LoginDisplayMojo::ShowWhitelistCheckFailedError() {
  host_->ShowWhitelistCheckFailedError();
}

void LoginDisplayMojo::ShowUnrecoverableCrypthomeErrorDialog() {
  host_->ShowUnrecoverableCrypthomeErrorDialog();
}

void LoginDisplayMojo::Login(const UserContext& user_context,
                             const SigninSpecifics& specifics) {
  if (host_)
    host_->existing_user_controller()->Login(user_context, specifics);
}

bool LoginDisplayMojo::IsSigninInProgress() const {
  NOTIMPLEMENTED();
  return false;
}

void LoginDisplayMojo::Signout() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::OnSigninScreenReady() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowEnterpriseEnrollmentScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowEnableDebuggingScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowKioskEnableScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowKioskAutolaunchScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowWrongHWIDScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowUpdateRequiredScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::CancelUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::RemoveUser(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
}

bool LoginDisplayMojo::IsShowGuest() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::IsShowUsers() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::ShowUsersHasChanged() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::IsAllowNewUser() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::AllowNewUserChanged() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::IsUserSigninCompleted() const {
  NOTIMPLEMENTED();
  return false;
}

void LoginDisplayMojo::HandleGetUsers() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::CheckUserStatus(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::OnUserImageChanged(const user_manager::User& user) {
  LoginScreenClient::Get()->login_screen()->SetAvatarForUser(
      user.GetAccountId(),
      UserSelectionScreen::BuildMojoUserAvatarForUser(&user));
}

}  // namespace chromeos
