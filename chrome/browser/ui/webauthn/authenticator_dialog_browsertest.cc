// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/webauthn/authenticator_request_dialog.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"

class AuthenticatorDialogTest : public DialogBrowserTest {
 public:
  AuthenticatorDialogTest() = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto model = std::make_unique<AuthenticatorRequestDialogModel>();

    // The dialog should immediately close as soon as it is displayed.
    if (name == "completed") {
      model->SetCurrentStep(AuthenticatorRequestDialogModel::Step::kCompleted);
    } else if (name == "transports") {
      TransportListModel* transports = model->transport_list_model();
      transports->AppendTransport(AuthenticatorTransport::kBluetoothLowEnergy);
      transports->AppendTransport(AuthenticatorTransport::kUsb);
      transports->AppendTransport(
          AuthenticatorTransport::kNearFieldCommunication);
      transports->AppendTransport(AuthenticatorTransport::kInternal);
      transports->AppendTransport(
          AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy);
      model->SetCurrentStep(
          AuthenticatorRequestDialogModel::Step::kTransportSelection);
    }

    ShowAuthenticatorRequestDialog(
        browser()->tab_strip_model()->GetActiveWebContents(), std::move(model));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorDialogTest);
};

// Run with:
//   --gtest_filter=BrowserUiTest.Invoke --test-launcher-interactive \
//   --ui=AuthenticatorDialogTest.InvokeUi_default
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_completed) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogTest, InvokeUi_transports) {
  ShowAndVerifyUi();
}
