// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace chromeos {

constexpr char kIsConfirmationDialogHiddenQuery[] =
    "!document.querySelector('.cr-dialog-container') || "
    "!!document.querySelector('.cr-dialog-container').hidden";

constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);

namespace {

// Waits for js condition to be fulfilled.
class JsConditionWaiter {
 public:
  JsConditionWaiter(const test::JSChecker& js_checker,
                    const std::string& js_condition)
      : js_checker_(js_checker), js_condition_(js_condition) {}

  ~JsConditionWaiter() = default;

  void Wait() {
    if (IsConditionFulfilled())
      return;

    timer_.Start(FROM_HERE, kJsConditionCheckFrequency, this,
                 &JsConditionWaiter::CheckCondition);
    run_loop_.Run();
  }

 private:
  bool IsConditionFulfilled() { return js_checker_.GetBool(js_condition_); }

  void CheckCondition() {
    if (IsConditionFulfilled()) {
      run_loop_.Quit();
      timer_.Stop();
    }
  }

  test::JSChecker js_checker_;
  const std::string js_condition_;

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

}  // namespace

// Basic tests for demo mode setup flow.
class DemoSetupTest : public LoginManagerTest {
 public:
  DemoSetupTest() : LoginManagerTest(false) {}
  ~DemoSetupTest() override = default;

  // LoginTestManager:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableDemoMode);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    DisableConfirmationDialogAnimations();
  }

  bool IsDemoSetupShown() {
    return js_checker().GetBool(
        "!!document.querySelector('#demo-setup') && "
        "!document.querySelector('#demo-setup').hidden");
  }

  bool IsConfirmationDialogShown() {
    return !js_checker().GetBool(kIsConfirmationDialogHiddenQuery);
  }

  void InvokeDemoMode() {
    EXPECT_TRUE(JSExecute("cr.ui.Oobe.handleAccelerator('demo_mode');"));
  }

  void ClickOkOnConfirmationDialog() {
    EXPECT_TRUE(JSExecute("document.querySelector('.cr-dialog-ok').click();"));
  }

  void ClickCancelOnConfirmationDialog() {
    EXPECT_TRUE(
        JSExecute("document.querySelector('.cr-dialog-cancel').click();"));
  }

  DemoSetupScreen* GetDemoSetupScreen() {
    return static_cast<DemoSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            OobeScreen::SCREEN_OOBE_DEMO_SETUP));
  }

 private:
  void DisableConfirmationDialogAnimations() {
    EXPECT_TRUE(
        JSExecute("cr.ui.dialogs.BaseDialog.ANIMATE_STABLE_DURATION = 0;"));
  }

  bool JSExecute(const std::string& script) {
    return content::ExecuteScript(web_contents(), script);
  }

  DISALLOW_COPY_AND_ASSIGN(DemoSetupTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowConfirmationDialogAndProceed) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoMode();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickOkOnConfirmationDialog();
  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_TRUE(IsDemoSetupShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowConfirmationDialogAndCancel) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoMode();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickCancelOnConfirmationDialog();
  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_FALSE(IsDemoSetupShown());
}

}  // namespace chromeos
