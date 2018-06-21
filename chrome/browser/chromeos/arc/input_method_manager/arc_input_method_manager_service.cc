// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"

namespace arc {

namespace {

// The Android IME id of the pre-installed IME to proxy Chrome OS IME's actions
// to inside the container.
// Please refer to ArcImeService for the implementation details.
constexpr char kChromeOSIMEIdInArcContainer[] =
    "org.chromium.arc.ime/.ArcInputMethodService";

// The name of the proxy IME extension that is used when registering ARC IMEs to
// InputMethodManager.
constexpr char kArcIMEProxyExtensionName[] =
    "org.chromium.arc.inputmethod.proxy";

// Singleton factory for ArcInputMethodManagerService
class ArcInputMethodManagerServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputMethodManagerService,
          ArcInputMethodManagerServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase
  static constexpr const char* kName = "ArcInputMethodManagerServiceFactory";

  static ArcInputMethodManagerServiceFactory* GetInstance() {
    return base::Singleton<ArcInputMethodManagerServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcInputMethodManagerServiceFactory>;
  ArcInputMethodManagerServiceFactory() = default;
  ~ArcInputMethodManagerServiceFactory() override = default;
};

}  // namespace

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContext(context);
}

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContextForTesting(
      context);
}

ArcInputMethodManagerService::ArcInputMethodManagerService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : profile_(Profile::FromBrowserContext(context)),
      imm_bridge_(
          std::make_unique<ArcInputMethodManagerBridgeImpl>(this,
                                                            bridge_service)),
      proxy_ime_extension_id_(
          crx_file::id_util::GenerateId(kArcIMEProxyExtensionName)),
      proxy_ime_engine_(std::make_unique<chromeos::InputMethodEngine>()) {
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->AddObserver(this);
  imm->AddImeMenuObserver(this);
}

ArcInputMethodManagerService::~ArcInputMethodManagerService() {
  // Remove any Arc IME entry from preferences before shutting down.
  // IME states (installed/enabled/disabled) are stored in Android's settings,
  // that will be restored after Arc container starts next time.
  RemoveArcIMEFromPrefs();
  profile_->GetPrefs()->CommitPendingWrite();

  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->RemoveImeMenuObserver(this);
  imm->RemoveObserver(this);
}

void ArcInputMethodManagerService::SetInputMethodManagerBridgeForTesting(
    std::unique_ptr<ArcInputMethodManagerBridge> test_bridge) {
  imm_bridge_ = std::move(test_bridge);
}

void ArcInputMethodManagerService::OnActiveImeChanged(
    const std::string& ime_id) {
  // Please see https://crbug.com/845079.
  NOTIMPLEMENTED();
}

void ArcInputMethodManagerService::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
  using namespace chromeos::input_method;

  if (!base::FeatureList::IsEnabled(kEnableInputMethodFeature))
    return;

  scoped_refptr<InputMethodManager::State> state =
      InputMethodManager::Get()->GetActiveIMEState();
  // Remove the old registered entry.
  state->RemoveInputMethodExtension(proxy_ime_extension_id_);

  // Convert ime_info_array to InputMethodDescriptors.
  InputMethodDescriptors descriptors;
  for (const auto& ime_info : ime_info_array)
    descriptors.push_back(BuildInputMethodDescriptor(ime_info.get()));
  if (descriptors.empty()) {
    // If no ARC IME is installed, remove ARC IME entry from preferences.
    RemoveArcIMEFromPrefs();
    return;
  }
  // Add the proxy IME entry to InputMethodManager if any ARC IME is installed.
  state->AddInputMethodExtension(proxy_ime_extension_id_, descriptors,
                                 proxy_ime_engine_.get());
}

void ArcInputMethodManagerService::ImeMenuListChanged() {
  auto* manager = chromeos::input_method::InputMethodManager::Get();
  auto new_active_ime_ids =
      manager->GetActiveIMEState()->GetActiveInputMethodIds();

  // Filter out non ARC IME ids.
  std::set<std::string> new_arc_active_ime_ids;
  std::copy_if(
      new_active_ime_ids.begin(), new_active_ime_ids.end(),
      std::inserter(new_arc_active_ime_ids, new_arc_active_ime_ids.end()),
      [](const auto& id) {
        return chromeos::extension_ime_util::IsArcIME(id);
      });

  for (const auto& id : new_arc_active_ime_ids) {
    // Enable the IME which is not currently enabled.
    if (!active_arc_ime_ids_.count(id))
      EnableIme(id, true /* enable */);
  }

  for (const auto& id : active_arc_ime_ids_) {
    // Disable the IME which is currently enabled.
    if (!new_arc_active_ime_ids.count(id))
      EnableIme(id, false /* enable */);
  }
  active_arc_ime_ids_.swap(new_arc_active_ime_ids);
}

void ArcInputMethodManagerService::InputMethodChanged(
    chromeos::input_method::InputMethodManager* manager,
    Profile* profile,
    bool /* show_message */) {
  scoped_refptr<chromeos::input_method::InputMethodManager::State> state =
      manager->GetActiveIMEState();
  if (!state)
    return;
  SwitchImeTo(state->GetCurrentInputMethod().id());
}

void ArcInputMethodManagerService::EnableIme(const std::string& ime_id,
                                             bool enable) {
  auto component_id =
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(ime_id);

  // TODO(yhanada): Disable the IME in Chrome OS side if it fails.
  imm_bridge_->SendEnableIme(
      component_id, enable,
      base::BindOnce(
          [](const std::string& ime_id, bool enable, bool success) {
            if (!success) {
              LOG(ERROR) << (enable ? "Enabling" : "Disabling") << " \""
                         << ime_id << "\" failed";
            }
          },
          ime_id, enable));
}

void ArcInputMethodManagerService::SwitchImeTo(const std::string& ime_id) {
  using namespace chromeos::extension_ime_util;

  std::string component_id = GetComponentIDByInputMethodID(ime_id);
  if (!IsArcIME(ime_id))
    component_id = kChromeOSIMEIdInArcContainer;
  imm_bridge_->SendSwitchImeTo(
      component_id, base::BindOnce(
                        [](const std::string& ime_id,
                           const std::string& component_id, bool success) {
                          if (!success) {
                            LOG(ERROR) << "Switch the active IME to \""
                                       << ime_id << "\"(component_id=\""
                                       << component_id << "\") failed";
                          }
                        },
                        ime_id, component_id));
}

chromeos::input_method::InputMethodDescriptor
ArcInputMethodManagerService::BuildInputMethodDescriptor(
    const mojom::ImeInfo* info) {
  // TODO(yhanada): Set the special layout/language value for ARC IMEs after
  // making settings and IME menu tray support it.
  std::vector<std::string> layouts{"us"};
  std::vector<std::string> languages{"en-US"};
  const std::string display_name = info->display_name;

  const std::string& input_method_id =
      chromeos::extension_ime_util::GetArcInputMethodID(proxy_ime_extension_id_,
                                                        info->ime_id);
  // TODO(yhanada): Set the indicator string after the UI spec is finalized.
  return chromeos::input_method::InputMethodDescriptor(
      input_method_id, display_name, std::string() /* indicator */, layouts,
      languages, false /* is_login_keyboard */, GURL(info->settings_url),
      GURL() /* input_view_url */);
}

void ArcInputMethodManagerService::RemoveArcIMEFromPrefs() {
  RemoveArcIMEFromPref(prefs::kLanguageEnabledExtensionImes);
  RemoveArcIMEFromPref(prefs::kLanguagePreloadEngines);

  PrefService* prefs = profile_->GetPrefs();
  if (chromeos::extension_ime_util::IsArcIME(
          prefs->GetString(prefs::kLanguageCurrentInputMethod))) {
    prefs->SetString(prefs::kLanguageCurrentInputMethod, std::string());
  }
  if (chromeos::extension_ime_util::IsArcIME(
          prefs->GetString(prefs::kLanguagePreviousInputMethod))) {
    prefs->SetString(prefs::kLanguagePreviousInputMethod, std::string());
  }
}

void ArcInputMethodManagerService::RemoveArcIMEFromPref(const char* pref_name) {
  const std::string ime_ids = profile_->GetPrefs()->GetString(pref_name);
  std::vector<std::string> ime_id_list = base::SplitString(
      ime_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  auto iter = std::remove_if(
      ime_id_list.begin(), ime_id_list.end(), [](const std::string& id) {
        return chromeos::extension_ime_util::IsArcIME(id);
      });
  ime_id_list.erase(iter, ime_id_list.end());

  profile_->GetPrefs()->SetString(pref_name,
                                  base::JoinString(ime_id_list, ","));
}

}  // namespace arc
