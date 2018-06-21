// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/secure_channel_disconnector.h"
#include "components/cryptauth/secure_channel.h"

namespace chromeos {

namespace secure_channel {

// Concrete SecureChannelDisconnector implementation.
class SecureChannelDisconnectorImpl
    : public SecureChannelDisconnector,
      public cryptauth::SecureChannel::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<SecureChannelDisconnector> BuildInstance();

   private:
    static Factory* test_factory_;
  };

  ~SecureChannelDisconnectorImpl() override;

 private:
  SecureChannelDisconnectorImpl();

  // SecureChannelDisconnector:
  void DisconnectSecureChannel(
      std::unique_ptr<cryptauth::SecureChannel> channel_to_disconnect) override;

  // cryptauth::SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      cryptauth::SecureChannel* secure_channel,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status) override;

  base::flat_set<std::unique_ptr<cryptauth::SecureChannel>>
      disconnecting_channels_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelDisconnectorImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_DISCONNECTOR_IMPL_H_
