// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/get_more_screen_handler.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kJsScreenPath[] = "AssistantGetMoreScreen";

}  // namespace

namespace chromeos {

GetMoreScreenHandler::GetMoreScreenHandler(
    OnAssistantOptInScreenExitCallback callback)
    : BaseWebUIHandler(), exit_callback_(std::move(callback)) {
  set_call_js_prefix(kJsScreenPath);
}

GetMoreScreenHandler::~GetMoreScreenHandler() = default;

void GetMoreScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void GetMoreScreenHandler::RegisterMessages() {
  AddPrefixedCallback("userActed", &GetMoreScreenHandler::HandleUserAction);
}

void GetMoreScreenHandler::Initialize() {}

void GetMoreScreenHandler::HandleUserAction(const bool screen_context,
                                            const bool email_opted_in) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetBoolean(arc::prefs::kVoiceInteractionContextEnabled,
                    screen_context);

  DCHECK(exit_callback_);
  if (email_opted_in) {
    std::move(exit_callback_).Run(AssistantOptInScreenExitCode::EMAIL_OPTED_IN);
  } else {
    std::move(exit_callback_)
        .Run(AssistantOptInScreenExitCode::EMAIL_OPTED_OUT);
  }
}

}  // namespace chromeos
