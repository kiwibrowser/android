// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_connection_delegate.h"

#include "base/bind.h"
#include "base/logging.h"

namespace chromeos {

namespace secure_channel {

FakeConnectionDelegate::FakeConnectionDelegate() = default;

FakeConnectionDelegate::~FakeConnectionDelegate() = default;

mojom::ConnectionDelegatePtr FakeConnectionDelegate::GenerateInterfacePtr() {
  mojom::ConnectionDelegatePtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeConnectionDelegate::DisconnectGeneratedPtrs() {
  bindings_.CloseAllBindings();
}

void FakeConnectionDelegate::OnConnectionAttemptFailure(
    mojom::ConnectionAttemptFailureReason reason) {
  connection_attempt_failure_reason_ = reason;

  if (closure_for_next_delegate_callback_)
    std::move(closure_for_next_delegate_callback_).Run();
}

void FakeConnectionDelegate::OnConnection(
    mojom::ChannelPtr channel,
    mojom::MessageReceiverRequest message_receiver_request) {
  channel_ = std::move(channel);
  message_receiver_request_ = std::move(message_receiver_request);

  if (closure_for_next_delegate_callback_)
    std::move(closure_for_next_delegate_callback_).Run();
}

}  // namespace secure_channel

}  // namespace chromeos
