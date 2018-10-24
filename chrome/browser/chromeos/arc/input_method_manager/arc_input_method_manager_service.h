// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "components/arc/common/input_method_manager.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcInputMethodManagerService
    : public KeyedService,
      public ArcInputMethodManagerBridge::Delegate,
      public chromeos::input_method::InputMethodManager::ImeMenuObserver,
      public chromeos::input_method::InputMethodManager::Observer {
 public:
  // Returns the instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcInputMethodManagerService* GetForBrowserContext(
      content::BrowserContext* context);
  // Does the same as GetForBrowserContext() but for testing. Please refer to
  // ArcBrowserContextKeyedServiceFactoryBase for the difference between them.
  static ArcInputMethodManagerService* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcInputMethodManagerService(content::BrowserContext* context,
                               ArcBridgeService* bridge_service);
  ~ArcInputMethodManagerService() override;

  void SetInputMethodManagerBridgeForTesting(
      std::unique_ptr<ArcInputMethodManagerBridge> test_bridge);

  // ArcInputMethodManagerBridge::Delegate overrides:
  void OnActiveImeChanged(const std::string& ime_id) override;
  void OnImeInfoChanged(std::vector<mojom::ImeInfoPtr> ime_info_array) override;

  // chromeos::input_method::InputMethodManager::ImeMenuObserver overrides:
  void ImeMenuListChanged() override;
  void ImeMenuActivationChanged(bool is_active) override {}
  void ImeMenuItemsChanged(
      const std::string& engine_id,
      const std::vector<chromeos::input_method::InputMethodManager::MenuItem>&
          items) override {}

  // chromeos::input_method::InputMethodManager::Observer overrides:
  void InputMethodChanged(chromeos::input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

 private:
  void EnableIme(const std::string& ime_id, bool enable);
  void SwitchImeTo(const std::string& ime_id);
  chromeos::input_method::InputMethodDescriptor BuildInputMethodDescriptor(
      const mojom::ImeInfo* info);

  // Removes ARC IME from IME related prefs that are current active IME pref,
  // previous active IME pref, enabled IME list pref and preloading IME list
  // pref.
  void RemoveArcIMEFromPrefs();
  void RemoveArcIMEFromPref(const char* pref_name);

  Profile* const profile_;

  std::unique_ptr<ArcInputMethodManagerBridge> imm_bridge_;
  std::set<std::string> active_arc_ime_ids_;

  // ArcInputMethodManager installs a proxy IME to redirect IME related events
  // from/to ARC IMEs in the container. The below two variables are for the
  // proxy IME.
  const std::string proxy_ime_extension_id_;
  std::unique_ptr<chromeos::InputMethodEngine> proxy_ime_engine_;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodManagerService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_SERVICE_H_
