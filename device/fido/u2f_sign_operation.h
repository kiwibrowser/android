// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_SIGN_OPERATION_H_
#define DEVICE_FIDO_U2F_SIGN_OPERATION_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/device_operation.h"
#include "device/fido/fido_constants.h"

namespace device {

class FidoDevice;
class CtapGetAssertionRequest;
class AuthenticatorGetAssertionResponse;
class PublicKeyCredentialDescriptor;

// Represents per device authentication logic for U2F tokens. Handles iterating
// through credentials in the allowed list, invokes check-only sign, and if
// check-only sign returns success, invokes regular sign call with user
// presence enforced.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#using-the-ctap2-authenticatorgetassertion-command-with-ctap1-u2f-authenticators
class COMPONENT_EXPORT(DEVICE_FIDO) U2fSignOperation
    : public DeviceOperation<CtapGetAssertionRequest,
                             AuthenticatorGetAssertionResponse> {
 public:
  U2fSignOperation(FidoDevice* device,
                   const CtapGetAssertionRequest& request,
                   DeviceResponseCallback callback);
  ~U2fSignOperation() override;

  // DeviceOperation:
  void Start() override;

 private:
  using AllowedListIterator =
      std::vector<PublicKeyCredentialDescriptor>::const_iterator;

  void RetrySign(bool is_fake_enrollment,
                 ApplicationParameterType application_parameter_type,
                 const std::vector<uint8_t>& key_handle);
  void OnSignResponseReceived(
      bool is_fake_enrollment,
      ApplicationParameterType application_parameter_type,
      const std::vector<uint8_t>& key_handle,
      base::Optional<std::vector<uint8_t>> device_response);
  void OnCheckForKeyHandlePresence(
      ApplicationParameterType application_parameter_type,
      AllowedListIterator it,
      base::Optional<std::vector<uint8_t>> device_response);

  base::WeakPtrFactory<U2fSignOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fSignOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_SIGN_OPERATION_H_
