// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_GAMEPAD_CONTROLLER_H_
#define CONTENT_SHELL_TEST_RUNNER_GAMEPAD_CONTROLLER_H_

#include <bitset>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"

namespace blink {
class WebLocalFrame;
}

namespace test_runner {

class TEST_RUNNER_EXPORT GamepadController
    : public device::mojom::GamepadMonitor,
      public base::SupportsWeakPtr<GamepadController> {
 public:
  GamepadController();
  ~GamepadController() override;

  void Reset();
  void Install(blink::WebLocalFrame* frame);

  void GamepadStartPolling(GamepadStartPollingCallback callback) override;
  void GamepadStopPolling(GamepadStopPollingCallback callback) override;
  void SetObserver(device::mojom::GamepadObserverPtr observer) override;

 private:
  friend class GamepadControllerBindings;

  // TODO(b.kelemen): for historical reasons Connect just initializes the
  // object. The 'gamepadconnected' event will be dispatched via
  // DispatchConnected. Tests for connected events need to first connect(),
  // then set the gamepad data and finally call dispatchConnected().
  // We should consider renaming Connect to Init and DispatchConnected to
  // Connect and at the same time updating all the gamepad tests.
  void Connect(int index);
  void DispatchConnected(int index);
  void Disconnect(int index);

  void SetId(int index, const std::string& src);
  void SetButtonCount(int index, int buttons);
  void SetButtonData(int index, int button, double data);
  void SetAxisCount(int index, int axes);
  void SetAxisData(int index, int axis, double data);
  void SetDualRumbleVibrationActuator(int index, bool enabled);

  void OnInterfaceRequest(mojo::ScopedMessagePipeHandle handle);

  device::mojom::GamepadObserverPtr observer_;
  mojo::Binding<device::mojom::GamepadMonitor> binding_;
  mojo::ScopedSharedBufferHandle buffer_;
  mojo::ScopedSharedBufferMapping mapping_;

  device::GamepadHardwareBuffer* gamepads_;
  std::bitset<device::Gamepads::kItemsLengthCap> missed_dispatches_;

  base::WeakPtrFactory<GamepadController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GamepadController);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_GAMEPAD_CONTROLLER_H_
