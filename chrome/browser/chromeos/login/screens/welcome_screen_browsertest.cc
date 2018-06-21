// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/welcome_screen.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/mock_network_state_helper.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_base_screen_delegate.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/views/controls/button/button.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnRef;
using views::Button;

namespace chromeos {

class DummyButtonListener : public views::ButtonListener {
 public:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {}
};

class WelcomeScreenTest : public WizardInProcessBrowserTest {
 public:
  WelcomeScreenTest()
      : WizardInProcessBrowserTest(OobeScreen::SCREEN_OOBE_WELCOME),
        fake_session_manager_client_(nullptr) {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    WizardInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  void SetUpOnMainThread() override {
    WizardInProcessBrowserTest::SetUpOnMainThread();
    mock_base_screen_delegate_.reset(new MockBaseScreenDelegate());
    ASSERT_TRUE(WizardController::default_controller() != nullptr);
    welcome_screen_ = WelcomeScreen::Get(
        WizardController::default_controller()->screen_manager());
    ASSERT_TRUE(welcome_screen_ != nullptr);
    ASSERT_EQ(WizardController::default_controller()->current_screen(),
              welcome_screen_);
    welcome_screen_->base_screen_delegate_ = mock_base_screen_delegate_.get();
    ASSERT_TRUE(welcome_screen_->view_ != nullptr);

    mock_network_state_helper_ = new login::MockNetworkStateHelper;
    SetDefaultNetworkStateHelperExpectations();
    welcome_screen_->SetNetworkStateHelperForTest(mock_network_state_helper_);
  }

  void EmulateContinueButtonExit(WelcomeScreen* welcome_screen) {
    EXPECT_CALL(*mock_base_screen_delegate_,
                OnExit(_, ScreenExitCode::NETWORK_CONNECTED, _))
        .Times(1);
    EXPECT_CALL(*mock_network_state_helper_, IsConnected())
        .WillOnce(Return(true));
    welcome_screen->OnContinueButtonPressed();
    content::RunAllPendingInMessageLoop();
  }

  void SetDefaultNetworkStateHelperExpectations() {
    EXPECT_CALL(*mock_network_state_helper_, GetCurrentNetworkName())
        .Times(AnyNumber())
        .WillRepeatedly((Return(base::string16())));
    EXPECT_CALL(*mock_network_state_helper_, IsConnected())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
    EXPECT_CALL(*mock_network_state_helper_, IsConnecting())
        .Times(AnyNumber())
        .WillRepeatedly((Return(false)));
  }

  std::unique_ptr<MockBaseScreenDelegate> mock_base_screen_delegate_;
  login::MockNetworkStateHelper* mock_network_state_helper_;
  WelcomeScreen* welcome_screen_;
  FakeSessionManagerClient* fake_session_manager_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WelcomeScreenTest);
};

IN_PROC_BROWSER_TEST_F(WelcomeScreenTest, CanConnect) {
  EXPECT_CALL(*mock_network_state_helper_, IsConnecting())
      .WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  welcome_screen_->UpdateStatus();

  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(2)
      .WillRepeatedly(Return(true));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  welcome_screen_->UpdateStatus();

  // EXPECT_TRUE(view_->IsContinueEnabled());
  EmulateContinueButtonExit(welcome_screen_);
}

IN_PROC_BROWSER_TEST_F(WelcomeScreenTest, Timeout) {
  EXPECT_CALL(*mock_network_state_helper_, IsConnecting())
      .WillOnce((Return(true)));
  // EXPECT_FALSE(view_->IsContinueEnabled());
  welcome_screen_->UpdateStatus();

  EXPECT_CALL(*mock_network_state_helper_, IsConnected())
      .Times(2)
      .WillRepeatedly(Return(false));
  // TODO(nkostylev): Add integration with WebUI view http://crosbug.com/22570
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  welcome_screen_->OnConnectionTimeout();

  // Close infobubble with error message - it makes the test stable.
  // EXPECT_FALSE(view_->IsContinueEnabled());
  // EXPECT_FALSE(view_->IsConnecting());
  // view_->ClearErrors();
}

}  // namespace chromeos
