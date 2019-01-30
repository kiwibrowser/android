// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge.h"

namespace arc {

class ArcBridgeService;

class ArcInputMethodManagerBridgeImpl : public ArcInputMethodManagerBridge,
                                        public mojom::InputMethodManagerHost {
 public:
  ArcInputMethodManagerBridgeImpl(Delegate* delegate,
                                  ArcBridgeService* bridge_service);
  ~ArcInputMethodManagerBridgeImpl() override;

  // ArcInputMethodManagerBridge overrides:
  void SendEnableIme(const std::string& ime_id,
                     bool enable,
                     EnableImeCallback callback) override;
  void SendSwitchImeTo(const std::string& ime_id,
                       SwitchImeToCallback callback) override;

  // mojom::InputMethodManagerHost overrides:
  void OnActiveImeChanged(const std::string& ime_id) override;
  void OnImeInfoChanged(std::vector<mojom::ImeInfoPtr> ime_info_array) override;

 private:
  Delegate* const delegate_;
  ArcBridgeService* const bridge_service_;  // Owned by ArcServiceManager

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodManagerBridgeImpl);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_IMPL_H_
