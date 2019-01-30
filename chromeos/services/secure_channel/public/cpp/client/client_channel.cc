// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/client_channel.h"

#include "base/callback.h"
#include "base/guid.h"

namespace chromeos {

namespace secure_channel {

ClientChannel::Observer::~Observer() = default;

ClientChannel::ClientChannel() = default;

ClientChannel::~ClientChannel() = default;

bool ClientChannel::GetConnectionMetadata(
    base::OnceCallback<void(mojom::ConnectionMetadataPtr)> callback) {
  if (is_disconnected_)
    return false;

  PerformGetConnectionMetadata(std::move(callback));
  return true;
}

bool ClientChannel::SendMessage(const std::string& payload,
                                base::OnceClosure on_sent_callback) {
  if (is_disconnected_)
    return false;

  PerformSendMessage(payload, std::move(on_sent_callback));
  return true;
}

void ClientChannel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ClientChannel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ClientChannel::NotifyDisconnected() {
  is_disconnected_ = true;

  for (auto& observer : observer_list_)
    observer.OnDisconnected();
}

void ClientChannel::NotifyMessageReceived(const std::string& payload) {
  // Make a copy before notifying observers to ensure that if one observer
  // deletes |this| before the next observer is able to be processed, a segfault
  // is prevented.
  const std::string payload_copy = payload;

  for (auto& observer : observer_list_)
    observer.OnMessageReceived(payload_copy);
}

}  // namespace secure_channel

}  // namespace chromeos
