// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/third_party_screen_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

constexpr char kJsScreenPath[] = "AssistantThirdPartyScreen";

constexpr char kUserActionNextPressed[] = "next-pressed";

}  // namespace

namespace chromeos {

ThirdPartyScreenHandler::ThirdPartyScreenHandler(
    OnAssistantOptInScreenExitCallback callback)
    : BaseWebUIHandler(), exit_callback_(std::move(callback)) {
  set_call_js_prefix(kJsScreenPath);
}

ThirdPartyScreenHandler::~ThirdPartyScreenHandler() = default;

void ThirdPartyScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void ThirdPartyScreenHandler::RegisterMessages() {
  AddPrefixedCallback("userActed", &ThirdPartyScreenHandler::HandleUserAction);
}

void ThirdPartyScreenHandler::Initialize() {}

void ThirdPartyScreenHandler::HandleUserAction(const std::string& action) {
  DCHECK(exit_callback_);
  if (action == kUserActionNextPressed) {
    std::move(exit_callback_)
        .Run(AssistantOptInScreenExitCode::THIRD_PARTY_CONTINUED);
  }
}

}  // namespace chromeos
