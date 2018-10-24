// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
#define CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_

#include <memory>

#include "base/macros.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/buffer.h"
#include "third_party/blink/public/platform/web_gamepad_listener.h"

namespace content {

class GamepadSharedMemoryReader : public device::mojom::GamepadObserver {
 public:
  GamepadSharedMemoryReader();
  ~GamepadSharedMemoryReader() override;

  void SampleGamepads(device::Gamepads& gamepads);
  void Start(blink::WebGamepadListener* listener);
  void Stop();

 protected:
  void SendStartMessage();
  void SendStopMessage();

 private:
  // device::mojom::GamepadObserver methods.
  void GamepadConnected(int index, const device::Gamepad& gamepad) override;
  void GamepadDisconnected(int index, const device::Gamepad& gamepad) override;

  mojo::ScopedSharedBufferHandle renderer_shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping renderer_shared_buffer_mapping_;
  device::GamepadHardwareBuffer* gamepad_hardware_buffer_ = nullptr;

  bool ever_interacted_with_ = false;

  mojo::Binding<device::mojom::GamepadObserver> binding_;
  device::mojom::GamepadMonitorPtr gamepad_monitor_;
  blink::WebGamepadListener* listener_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GamepadSharedMemoryReader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_GAMEPAD_SHARED_MEMORY_READER_H_
