// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_channel.h"

namespace chromeos {

namespace secure_channel {

namespace {
const char kDisconnectionDescription[] = "Remote device disconnected.";
}  // namespace

FakeChannel::FakeChannel() : binding_(this) {}

FakeChannel::~FakeChannel() = default;

mojom::ChannelPtr FakeChannel::GenerateInterfacePtr() {
  mojom::ChannelPtr interface_ptr;
  binding_.Bind(mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeChannel::DisconnectGeneratedPtr() {
  binding_.CloseWithReason(mojom::Channel::kConnectionDroppedReason,
                           kDisconnectionDescription);
}

void FakeChannel::SendMessage(const std::string& message,
                              SendMessageCallback callback) {
  sent_messages_.push_back(std::make_pair(message, std::move(callback)));
}

void FakeChannel::GetConnectionMetadata(
    GetConnectionMetadataCallback callback) {
  std::move(callback).Run(std::move(connection_metadata_for_next_call_));
}

}  // namespace secure_channel

}  // namespace chromeos
