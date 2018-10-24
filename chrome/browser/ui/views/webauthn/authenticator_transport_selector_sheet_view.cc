// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/authenticator_transport_selector_sheet_view.h"

#include <utility>

AuthenticatorTransportSelectorSheetView::
    AuthenticatorTransportSelectorSheetView(
        std::unique_ptr<AuthenticatorTransportSelectorSheetModel> model)
    : AuthenticatorRequestSheetView(std::move(model)) {}
AuthenticatorTransportSelectorSheetView::
    ~AuthenticatorTransportSelectorSheetView() = default;

std::unique_ptr<views::View>
AuthenticatorTransportSelectorSheetView::BuildStepSpecificContent() {
  return std::make_unique<TransportListView>(
      model()->dialog_model()->transport_list_model(), this);
}

void AuthenticatorTransportSelectorSheetView::OnListItemSelected(
    AuthenticatorTransport transport) {
  model()->OnTransportSelected(transport);
}
