// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_base.h"
#include "components/cryptauth/remote_device_cache.h"

namespace chromeos {

namespace secure_channel {

// Test double SecureChannel implementation.
class FakeSecureChannel : public SecureChannelBase {
 public:
  FakeSecureChannel();
  ~FakeSecureChannel() override;

  mojom::ConnectionDelegatePtr delegate_from_last_listen_call() {
    return std::move(delegate_from_last_listen_call_);
  }

  mojom::ConnectionDelegatePtr delegate_from_last_initiate_call() {
    return std::move(delegate_from_last_initiate_call_);
  }

 private:
  // mojom::SecureChannel:
  void ListenForConnectionFromDevice(
      const cryptauth::RemoteDevice& device_to_connect,
      const cryptauth::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionDelegatePtr delegate) override;
  void InitiateConnectionToDevice(
      const cryptauth::RemoteDevice& device_to_connect,
      const cryptauth::RemoteDevice& local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionDelegatePtr delegate) override;

  mojom::ConnectionDelegatePtr delegate_from_last_listen_call_;
  mojom::ConnectionDelegatePtr delegate_from_last_initiate_call_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecureChannel);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_SECURE_CHANNEL_H_