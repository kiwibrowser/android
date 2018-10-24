// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/gamepad_controller.h"

#include <string.h>

#include "base/macros.h"
#include "content/public/common/service_names.mojom.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/web_gamepad_listener.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

using device::Gamepad;
using device::Gamepads;

namespace test_runner {

class GamepadControllerBindings
    : public gin::Wrappable<GamepadControllerBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<GamepadController> controller,
                      blink::WebLocalFrame* frame);

 private:
  explicit GamepadControllerBindings(
      base::WeakPtr<GamepadController> controller);
  ~GamepadControllerBindings() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  void Connect(int index);
  void DispatchConnected(int index);
  void Disconnect(int index);
  void SetId(int index, const std::string& src);
  void SetButtonCount(int index, int buttons);
  void SetButtonData(int index, int button, double data);
  void SetAxisCount(int index, int axes);
  void SetAxisData(int index, int axis, double data);
  void SetDualRumbleVibrationActuator(int index, bool enabled);

  base::WeakPtr<GamepadController> controller_;

  DISALLOW_COPY_AND_ASSIGN(GamepadControllerBindings);
};

gin::WrapperInfo GamepadControllerBindings::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void GamepadControllerBindings::Install(
    base::WeakPtr<GamepadController> controller,
    blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<GamepadControllerBindings> bindings =
      gin::CreateHandle(isolate, new GamepadControllerBindings(controller));
  if (bindings.IsEmpty())
    return;
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "gamepadController"), bindings.ToV8());
}

GamepadControllerBindings::GamepadControllerBindings(
    base::WeakPtr<GamepadController> controller)
    : controller_(controller) {}

GamepadControllerBindings::~GamepadControllerBindings() {}

gin::ObjectTemplateBuilder GamepadControllerBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<GamepadControllerBindings>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod("connect", &GamepadControllerBindings::Connect)
      .SetMethod("dispatchConnected",
                 &GamepadControllerBindings::DispatchConnected)
      .SetMethod("disconnect", &GamepadControllerBindings::Disconnect)
      .SetMethod("setId", &GamepadControllerBindings::SetId)
      .SetMethod("setButtonCount", &GamepadControllerBindings::SetButtonCount)
      .SetMethod("setButtonData", &GamepadControllerBindings::SetButtonData)
      .SetMethod("setAxisCount", &GamepadControllerBindings::SetAxisCount)
      .SetMethod("setAxisData", &GamepadControllerBindings::SetAxisData)
      .SetMethod("setDualRumbleVibrationActuator",
                 &GamepadControllerBindings::SetDualRumbleVibrationActuator);
}

void GamepadControllerBindings::Connect(int index) {
  if (controller_)
    controller_->Connect(index);
}

void GamepadControllerBindings::DispatchConnected(int index) {
  if (controller_)
    controller_->DispatchConnected(index);
}

void GamepadControllerBindings::Disconnect(int index) {
  if (controller_)
    controller_->Disconnect(index);
}

void GamepadControllerBindings::SetId(int index, const std::string& src) {
  if (controller_)
    controller_->SetId(index, src);
}

void GamepadControllerBindings::SetButtonCount(int index, int buttons) {
  if (controller_)
    controller_->SetButtonCount(index, buttons);
}

void GamepadControllerBindings::SetButtonData(int index,
                                              int button,
                                              double data) {
  if (controller_)
    controller_->SetButtonData(index, button, data);
}

void GamepadControllerBindings::SetAxisCount(int index, int axes) {
  if (controller_)
    controller_->SetAxisCount(index, axes);
}

void GamepadControllerBindings::SetAxisData(int index, int axis, double data) {
  if (controller_)
    controller_->SetAxisData(index, axis, data);
}

void GamepadControllerBindings::SetDualRumbleVibrationActuator(int index,
                                                               bool enabled) {
  if (controller_)
    controller_->SetDualRumbleVibrationActuator(index, enabled);
}

GamepadController::GamepadController()
    : observer_(nullptr), binding_(this), weak_factory_(this) {
  size_t buffer_size = sizeof(device::GamepadHardwareBuffer);
  buffer_ = mojo::SharedBufferHandle::Create(buffer_size);
  CHECK(buffer_.is_valid());
  mapping_ = buffer_->Map(buffer_size);
  CHECK(mapping_);
  gamepads_ = new (mapping_.get()) device::GamepadHardwareBuffer();

  Reset();
}

GamepadController::~GamepadController() {}

void GamepadController::Reset() {
  memset(gamepads_, 0, sizeof(*gamepads_));
  missed_dispatches_.reset();
}

void GamepadController::Install(blink::WebLocalFrame* frame) {
  service_manager::Connector::TestApi connector_test_api(
      blink::Platform::Current()->GetConnector());
  connector_test_api.OverrideBinderForTesting(
      service_manager::Identity(content::mojom::kBrowserServiceName),
      device::mojom::GamepadMonitor::Name_,
      base::BindRepeating(&GamepadController::OnInterfaceRequest,
                          base::Unretained(this)));

  GamepadControllerBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void GamepadController::OnInterfaceRequest(
    mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(device::mojom::GamepadMonitorRequest(std::move(handle)));
  observer_ = nullptr;
}

void GamepadController::GamepadStartPolling(
    GamepadStartPollingCallback callback) {
  std::move(callback).Run(
      buffer_->Clone(mojo::SharedBufferHandle::AccessMode::READ_ONLY));
}

void GamepadController::GamepadStopPolling(
    GamepadStopPollingCallback callback) {
  std::move(callback).Run();
}

void GamepadController::SetObserver(
    device::mojom::GamepadObserverPtr observer) {
  observer_ = std::move(observer);

  // Notify the new observer of any GamepadConnected RPCs that it missed because
  // the SetObserver RPC wasn't processed in time. This happens during layout
  // tests because SetObserver is async, so the test can continue to the
  // DispatchConnected call before the SetObserver RPC was processed. This isn't
  // an issue in the real implementation because the 'gamepadconnected' event
  // doesn't fire until user input is detected, so even if a GamepadConnected
  // event is missed, another will be picked up after the next user input.
  gamepads_->seqlock.WriteBegin();
  for (size_t index = 0; index < Gamepads::kItemsLengthCap; index++) {
    if (missed_dispatches_.test(index)) {
      observer_->GamepadConnected(index, gamepads_->data.items[index]);
    }
  }
  missed_dispatches_.reset();
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::Connect(int index) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  gamepads_->data.items[index].connected = true;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::DispatchConnected(int index) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap) ||
      !gamepads_->data.items[index].connected)
    return;
  gamepads_->seqlock.WriteBegin();
  const Gamepad& pad = gamepads_->data.items[index];
  if (observer_) {
    observer_->GamepadConnected(index, pad);
  } else {
    // Record that there wasn't an observer to get the GamepadConnected RPC so
    // we can send it when SetObserver gets called.
    missed_dispatches_.set(index);
  }
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::Disconnect(int index) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  Gamepad& pad = gamepads_->data.items[index];
  pad.connected = false;
  if (observer_)
    observer_->GamepadDisconnected(index, pad);
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetId(int index, const std::string& src) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  const char* p = src.c_str();
  gamepads_->seqlock.WriteBegin();
  memset(gamepads_->data.items[index].id, 0,
         sizeof(gamepads_->data.items[index].id));
  for (unsigned i = 0; *p && i < Gamepad::kIdLengthCap - 1; ++i)
    gamepads_->data.items[index].id[i] = *p++;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetButtonCount(int index, int buttons) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (buttons < 0 || buttons >= static_cast<int>(Gamepad::kButtonsLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  gamepads_->data.items[index].buttons_length = buttons;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetButtonData(int index, int button, double data) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (button < 0 || button >= static_cast<int>(Gamepad::kButtonsLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  gamepads_->data.items[index].buttons[button].value = data;
  gamepads_->data.items[index].buttons[button].pressed = data > 0.1f;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetAxisCount(int index, int axes) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (axes < 0 || axes >= static_cast<int>(Gamepad::kAxesLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  gamepads_->data.items[index].axes_length = axes;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetAxisData(int index, int axis, double data) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  if (axis < 0 || axis >= static_cast<int>(Gamepad::kAxesLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  gamepads_->data.items[index].axes[axis] = data;
  gamepads_->seqlock.WriteEnd();
}

void GamepadController::SetDualRumbleVibrationActuator(int index,
                                                       bool enabled) {
  if (index < 0 || index >= static_cast<int>(Gamepads::kItemsLengthCap))
    return;
  gamepads_->seqlock.WriteBegin();
  gamepads_->data.items[index].vibration_actuator.type =
      device::GamepadHapticActuatorType::kDualRumble;
  gamepads_->data.items[index].vibration_actuator.not_null = enabled;
  gamepads_->seqlock.WriteEnd();
}

}  // namespace test_runner
