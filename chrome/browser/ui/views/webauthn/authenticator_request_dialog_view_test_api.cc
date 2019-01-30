// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"

#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "components/constrained_window/constrained_window_views.h"

namespace test {

// static
void AuthenticatorRequestDialogViewTestApi::Show(
    content::WebContents* web_contents,
    std::unique_ptr<AuthenticatorRequestDialogView> dialog) {
  constrained_window::ShowWebModalDialogViews(dialog.release(), web_contents);
}

// static
void AuthenticatorRequestDialogViewTestApi::ReplaceCurrentSheet(
    AuthenticatorRequestDialogView* dialog,
    std::unique_ptr<AuthenticatorRequestSheetView> new_sheet) {
  dialog->ReplaceCurrentSheetWith(std::move(new_sheet));
}
}  // namespace test
