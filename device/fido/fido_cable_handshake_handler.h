// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_CABLE_HANDSHAKE_HANDLER_H_
#define DEVICE_FIDO_FIDO_CABLE_HANDSHAKE_HANDLER_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_device.h"

namespace device {

class FidoCableDevice;

// Handles exchanging handshake messages with external authenticator and
// validating the handshake messages to derive a shared session key to be used
// for message encryption.
// See: fido-client-to-authenticator-protocol.html#cable-encryption-handshake of
// the most up-to-date spec.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableHandshakeHandler {
 public:
  FidoCableHandshakeHandler(FidoCableDevice* device,
                            base::span<const uint8_t, 8> nonce,
                            base::span<const uint8_t, 32> session_pre_key);
  virtual ~FidoCableHandshakeHandler();

  virtual void InitiateCableHandshake(FidoDevice::DeviceCallback callback);
  virtual bool ValidateAuthenticatorHandshakeMessage(
      base::span<const uint8_t> response);

 private:
  FRIEND_TEST_ALL_PREFIXES(FidoCableHandshakeHandlerTest, HandShakeSuccess);
  FRIEND_TEST_ALL_PREFIXES(FidoCableHandshakeHandlerTest,
                           HandshakeFailWithIncorrectAuthenticatorResponse);

  std::string GetEncryptionKeyAfterSuccessfulHandshake(
      base::span<const uint8_t, 16> authenticator_random_nonce) const;

  FidoCableDevice* const cable_device_;
  std::array<uint8_t, 8> nonce_;
  std::array<uint8_t, 32> session_pre_key_;
  std::array<uint8_t, 16> client_session_random_;
  std::string handshake_key_;

  base::WeakPtrFactory<FidoCableHandshakeHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoCableHandshakeHandler);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_CABLE_HANDSHAKE_HANDLER_H_
