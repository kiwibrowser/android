// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_H_

#include "components/arc/common/input_method_manager.mojom.h"

namespace arc {

// The interface class encapsulates the detail of input method manager related
// IPC between Chrome and the ARC container.
class ArcInputMethodManagerBridge {
 public:
  virtual ~ArcInputMethodManagerBridge() = default;

  // Received mojo calls are passed to this delegate.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnActiveImeChanged(const std::string& ime_id) = 0;
    virtual void OnImeInfoChanged(
        const std::vector<mojom::ImeInfoPtr> ime_info_array) = 0;
  };

  // Sends mojo calls.
  using EnableImeCallback =
      mojom::InputMethodManagerInstance::EnableImeCallback;
  using SwitchImeToCallback =
      mojom::InputMethodManagerInstance::SwitchImeToCallback;

  virtual void SendEnableIme(const std::string& ime_id,
                             bool enable,
                             EnableImeCallback callback) = 0;
  virtual void SendSwitchImeTo(const std::string& ime_id,
                               SwitchImeToCallback callback) = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INPUT_METHOD_MANAGER_ARC_INPUT_METHOD_MANAGER_BRIDGE_H_
