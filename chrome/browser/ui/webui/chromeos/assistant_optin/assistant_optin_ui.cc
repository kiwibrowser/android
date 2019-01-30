// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/get_more_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/third_party_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/assistant_optin/value_prop_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

bool is_active = false;

constexpr int kAssistantOptInDialogWidth = 576;
constexpr int kAssistantOptInDialogHeight = 480;

// Construct SettingsUiSelector for the ConsentFlow UI.
assistant::SettingsUiSelector GetSettingsUiSelector() {
  assistant::SettingsUiSelector selector;
  assistant::ConsentFlowUiSelector* consent_flow_ui =
      selector.mutable_consent_flow_ui_selector();
  consent_flow_ui->set_flow_id(assistant::ActivityControlSettingsUiSelector::
                                   ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  selector.set_email_opt_in(true);
  return selector;
}

// Construct SettingsUiUpdate for user opt-in.
assistant::SettingsUiUpdate GetSettingsUiUpdate(
    const std::string& consent_token) {
  assistant::SettingsUiUpdate update;
  assistant::ConsentFlowUiUpdate* consent_flow_update =
      update.mutable_consent_flow_ui_update();
  consent_flow_update->set_flow_id(
      assistant::ActivityControlSettingsUiSelector::
          ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS);
  consent_flow_update->set_consent_token(consent_token);
  consent_flow_update->set_saw_third_party_disclosure(true);

  return update;
}

// Construct SettingsUiUpdate for email opt-in.
assistant::SettingsUiUpdate GetEmailOptInUpdate(bool opted_in) {
  assistant::SettingsUiUpdate update;
  assistant::EmailOptInUpdate* email_optin_update =
      update.mutable_email_opt_in_update();
  email_optin_update->set_email_opt_in_update_state(
      opted_in ? assistant::EmailOptInUpdate::OPT_IN
               : assistant::EmailOptInUpdate::OPT_OUT);

  return update;
}

using SettingZippyList = google::protobuf::RepeatedPtrField<
    assistant::ClassicActivityControlUiTexts::SettingZippy>;
// Helper method to create zippy data.
base::ListValue CreateZippyData(const SettingZippyList& zippy_list) {
  base::ListValue zippy_data;
  for (auto& setting_zippy : zippy_list) {
    base::DictionaryValue data;
    data.SetString("title", setting_zippy.title());
    if (setting_zippy.description_paragraph_size()) {
      data.SetString("description", setting_zippy.description_paragraph(0));
    }
    if (setting_zippy.additional_info_paragraph_size()) {
      data.SetString("additionalInfo",
                     setting_zippy.additional_info_paragraph(0));
    }
    data.SetString("iconUri", setting_zippy.icon_uri());
    zippy_data.GetList().push_back(std::move(data));
  }
  return zippy_data;
}

// Helper method to create disclosure data.
base::ListValue CreateDisclosureData(const SettingZippyList& disclosure_list) {
  base::ListValue disclosure_data;
  for (auto& disclosure : disclosure_list) {
    base::DictionaryValue data;
    data.SetString("title", disclosure.title());
    if (disclosure.description_paragraph_size()) {
      data.SetString("description", disclosure.description_paragraph(0));
    }
    if (disclosure.additional_info_paragraph_size()) {
      data.SetString("additionalInfo", disclosure.additional_info_paragraph(0));
    }
    data.SetString("iconUri", disclosure.icon_uri());
    disclosure_data.GetList().push_back(std::move(data));
  }
  return disclosure_data;
}

// Helper method to create get more screen data.
base::ListValue CreateGetMoreData(
    bool email_optin_needed,
    const assistant::EmailOptInUi& email_optin_ui) {
  base::ListValue get_more_data;

  // Process screen context data.
  base::DictionaryValue context_data;
  context_data.SetString(
      "title", l10n_util::GetStringUTF16(IDS_ASSISTANT_SCREEN_CONTEXT_TITLE));
  context_data.SetString("description", l10n_util::GetStringUTF16(
                                            IDS_ASSISTANT_SCREEN_CONTEXT_DESC));
  context_data.SetBoolean("defaultEnabled", true);
  context_data.SetString("iconUri",
                         "https://www.gstatic.com/images/icons/material/system/"
                         "2x/laptop_chromebook_grey600_24dp.png");
  get_more_data.GetList().push_back(std::move(context_data));

  // Process email optin data.
  if (email_optin_needed) {
    base::DictionaryValue data;
    data.SetString("title", email_optin_ui.title());
    data.SetString("description", email_optin_ui.description());
    data.SetBoolean("defaultEnabled", email_optin_ui.default_enabled());
    data.SetString("iconUri", email_optin_ui.icon_uri());
    get_more_data.GetList().push_back(std::move(data));
  }

  return get_more_data;
}

// Get string constants for settings ui.
base::DictionaryValue GetSettingsUiStrings(
    const assistant::SettingsUi& settings_ui,
    bool activity_controll_needed) {
  auto consent_ui = settings_ui.consent_flow_ui().consent_ui();
  auto activity_control_ui = consent_ui.activity_control_ui();
  auto third_party_disclosure_ui = consent_ui.third_party_disclosure_ui();
  base::DictionaryValue dictionary;

  // Add activity controll string constants.
  if (activity_controll_needed) {
    dictionary.SetString("valuePropIdentity", activity_control_ui.identity());
    if (activity_control_ui.intro_text_paragraph_size()) {
      dictionary.SetString("valuePropIntro",
                           activity_control_ui.intro_text_paragraph(0));
    }
    if (activity_control_ui.footer_paragraph_size()) {
      dictionary.SetString("valuePropFooter",
                           activity_control_ui.footer_paragraph(0));
    }
    dictionary.SetString("valuePropNextButton",
                         consent_ui.accept_button_text());
    dictionary.SetString("valuePropSkipButton",
                         consent_ui.reject_button_text());
  }

  // Add third party string constants.
  dictionary.SetString("thirdPartyTitle", third_party_disclosure_ui.title());
  dictionary.SetString("thirdPartyContinueButton",
                       third_party_disclosure_ui.button_continue());
  dictionary.SetString("thirdPartyFooter", consent_ui.tos_pp_links());

  // Add get more screen string constants.
  dictionary.SetString(
      "getMoreTitle",
      l10n_util::GetStringUTF16(IDS_ASSISTANT_GET_MORE_SCREEN_TITLE));
  dictionary.SetString(
      "getMoreContinueButton",
      l10n_util::GetStringUTF16(IDS_ASSISTANT_GET_MORE_SCREEN_CONTINUE_BUTTON));

  return dictionary;
}

}  // namespace

AssistantOptInUI::AssistantOptInUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  // Set up the chrome://assistant-optin source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAssistantOptInHost);

  js_calls_container_ = std::make_unique<JSCallsContainer>();

  auto base_handler =
      std::make_unique<AssistantOptInHandler>(js_calls_container_.get());
  assistant_handler_ = base_handler.get();
  AddScreenHandler(std::move(base_handler));

  AddScreenHandler(std::make_unique<ValuePropScreenHandler>(
      base::BindOnce(&AssistantOptInUI::OnExit, weak_factory_.GetWeakPtr())));
  AddScreenHandler(std::make_unique<ThirdPartyScreenHandler>(
      base::BindOnce(&AssistantOptInUI::OnExit, weak_factory_.GetWeakPtr())));
  AddScreenHandler(std::make_unique<GetMoreScreenHandler>(
      base::BindOnce(&AssistantOptInUI::OnExit, weak_factory_.GetWeakPtr())));

  base::DictionaryValue localized_strings;
  for (auto* handler : screen_handlers_)
    handler->GetLocalizedStrings(&localized_strings);
  source->AddLocalizedStrings(localized_strings);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("assistant_optin.js", IDR_ASSISTANT_OPTIN_JS);
  source->AddResourcePath("assistant_logo.png", IDR_ASSISTANT_LOGO_PNG);
  source->SetDefaultResource(IDR_ASSISTANT_OPTIN_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  if (arc::VoiceInteractionControllerClient::Get()->voice_interaction_state() !=
      ash::mojom::VoiceInteractionState::RUNNING) {
    arc::VoiceInteractionControllerClient::Get()->AddObserver(this);
  } else {
    Initialize();
  }
}

AssistantOptInUI::~AssistantOptInUI() {
  arc::VoiceInteractionControllerClient::Get()->RemoveObserver(this);
}

void AssistantOptInUI::OnStateChanged(ash::mojom::VoiceInteractionState state) {
  if (state == ash::mojom::VoiceInteractionState::RUNNING)
    Initialize();
}

void AssistantOptInUI::Initialize() {
  if (settings_manager_.is_bound())
    return;

  // Set up settings mojom.
  Profile* const profile = Profile::FromWebUI(web_ui());
  service_manager::Connector* connector =
      content::BrowserContext::GetConnectorFor(profile);
  connector->BindInterface(assistant::mojom::kServiceName,
                           mojo::MakeRequest(&settings_manager_));

  // Send GetSettings request for the ConsentFlow UI.
  assistant::SettingsUiSelector selector = GetSettingsUiSelector();
  settings_manager_->GetSettings(
      selector.SerializeAsString(),
      base::BindOnce(&AssistantOptInUI::OnGetSettingsResponse,
                     weak_factory_.GetWeakPtr()));
}

void AssistantOptInUI::AddScreenHandler(
    std::unique_ptr<BaseWebUIHandler> handler) {
  screen_handlers_.push_back(handler.get());
  web_ui()->AddMessageHandler(std::move(handler));
}

void AssistantOptInUI::OnExit(AssistantOptInScreenExitCode exit_code) {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  switch (exit_code) {
    case AssistantOptInScreenExitCode::VALUE_PROP_SKIPPED:
      prefs->SetBoolean(arc::prefs::kArcVoiceInteractionValuePropAccepted,
                        false);
      prefs->SetBoolean(arc::prefs::kVoiceInteractionEnabled, false);
      CloseDialog(nullptr);
      break;
    case AssistantOptInScreenExitCode::VALUE_PROP_ACCEPTED:
      assistant_handler_->ShowNextScreen();
      break;
    case AssistantOptInScreenExitCode::THIRD_PARTY_CONTINUED:
      if (activity_controll_needed_) {
        // Send the update to complete user opt-in.
        settings_manager_->UpdateSettings(
            GetSettingsUiUpdate(consent_token_).SerializeAsString(),
            base::BindOnce(&AssistantOptInUI::OnUpdateSettingsResponse,
                           weak_factory_.GetWeakPtr(), false));
      } else {
        assistant_handler_->ShowNextScreen();
      }
      break;
    case AssistantOptInScreenExitCode::EMAIL_OPTED_IN:
      DCHECK(email_optin_needed_);
      OnEmailOptInResult(true);
      break;
    case AssistantOptInScreenExitCode::EMAIL_OPTED_OUT:
      if (email_optin_needed_)
        OnEmailOptInResult(false);
      else
        CloseDialog(nullptr);
      break;
    default:
      NOTREACHED();
  }
}

void AssistantOptInUI::OnEmailOptInResult(bool opted_in) {
  settings_manager_->UpdateSettings(
      GetEmailOptInUpdate(opted_in).SerializeAsString(),
      base::BindOnce(&AssistantOptInUI::OnUpdateSettingsResponse,
                     weak_factory_.GetWeakPtr(), true));
}

void AssistantOptInUI::OnGetSettingsResponse(const std::string& settings) {
  assistant::SettingsUi settings_ui;
  settings_ui.ParseFromString(settings);

  DCHECK(settings_ui.has_consent_flow_ui());
  auto activity_control_ui =
      settings_ui.consent_flow_ui().consent_ui().activity_control_ui();
  auto third_party_disclosure_ui =
      settings_ui.consent_flow_ui().consent_ui().third_party_disclosure_ui();

  consent_token_ = activity_control_ui.consent_token();

  // Process activity controll data.
  if (!activity_control_ui.setting_zippy().size()) {
    // No need to consent. Move to the next screen.
    activity_controll_needed_ = false;
    PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
    prefs->SetBoolean(arc::prefs::kArcVoiceInteractionValuePropAccepted, true);
    prefs->SetBoolean(arc::prefs::kVoiceInteractionEnabled, true);
    assistant_handler_->ShowNextScreen();
  } else {
    assistant_handler_->AddSettingZippy(
        "settings", CreateZippyData(activity_control_ui.setting_zippy()));
  }

  // Process third party disclosure data.
  assistant_handler_->AddSettingZippy(
      "disclosure",
      CreateDisclosureData(third_party_disclosure_ui.disclosures()));

  // Process get more data.
  email_optin_needed_ = settings_ui.has_email_opt_in_ui() &&
                        settings_ui.email_opt_in_ui().has_title();
  assistant_handler_->AddSettingZippy(
      "get-more",
      CreateGetMoreData(email_optin_needed_, settings_ui.email_opt_in_ui()));

  // Pass string constants dictionary.
  assistant_handler_->ReloadContent(
      GetSettingsUiStrings(settings_ui, activity_controll_needed_));
}

void AssistantOptInUI::OnUpdateSettingsResponse(bool should_exit,
                                                const std::string& result) {
  assistant::SettingsUiUpdateResult ui_result;
  ui_result.ParseFromString(result);

  if (ui_result.has_consent_flow_update_result()) {
    if (ui_result.consent_flow_update_result().update_status() !=
        assistant::ConsentFlowUiUpdateResult::SUCCESS) {
      // TODO(updowndta): Handle consent update failure.
      LOG(ERROR) << "Consent udpate error.";
    } else {
      PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
      prefs->SetBoolean(arc::prefs::kArcVoiceInteractionValuePropAccepted,
                        true);
      prefs->SetBoolean(arc::prefs::kVoiceInteractionEnabled, true);
    }
  }

  if (ui_result.has_email_opt_in_update_result()) {
    if (ui_result.email_opt_in_update_result().update_status() !=
        assistant::EmailOptInUpdateResult::SUCCESS) {
      // TODO(updowndta): Handle email optin update failure.
      LOG(ERROR) << "Email OptIn udpate error.";
    }
  }

  if (!should_exit) {
    assistant_handler_->ShowNextScreen();
  } else {
    CloseDialog(nullptr);
  }
}

// AssistantOptInDialog

// static
void AssistantOptInDialog::Show() {
  DCHECK(!is_active);
  AssistantOptInDialog* dialog = new AssistantOptInDialog();
  dialog->ShowSystemDialog(true);
}

// static
bool AssistantOptInDialog::IsActive() {
  return is_active;
}

AssistantOptInDialog::AssistantOptInDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAssistantOptInURL),
                              base::string16()) {
  DCHECK(!is_active);
  is_active = true;
}

AssistantOptInDialog::~AssistantOptInDialog() {
  is_active = false;
}

void AssistantOptInDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kAssistantOptInDialogWidth, kAssistantOptInDialogHeight);
}

std::string AssistantOptInDialog::GetDialogArgs() const {
  return std::string();
}

bool AssistantOptInDialog::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace chromeos
