// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_REGISTER_OPERATION_H_
#define DEVICE_FIDO_U2F_REGISTER_OPERATION_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/fido/device_operation.h"

namespace device {

class FidoDevice;
class CtapMakeCredentialRequest;
class AuthenticatorMakeCredentialResponse;
class PublicKeyCredentialDescriptor;

// Represents per device registration logic for U2F tokens. Handles regular U2F
// registration as well as the logic of iterating key handles in the exclude
// list and conducting check-only U2F sign to prevent duplicate registration.
// U2fRegistrationOperation is owned by MakeCredentialTask and |request_| is
// also owned by MakeCredentialTask.
class COMPONENT_EXPORT(DEVICE_FIDO) U2fRegisterOperation
    : public DeviceOperation<CtapMakeCredentialRequest,
                             AuthenticatorMakeCredentialResponse> {
 public:
  U2fRegisterOperation(FidoDevice* device,
                       const CtapMakeCredentialRequest& request,
                       DeviceResponseCallback callback);
  ~U2fRegisterOperation() override;

  // DeviceOperation:
  void Start() override;

 private:
  using ExcludeListIterator =
      std::vector<PublicKeyCredentialDescriptor>::const_iterator;

  void TryRegistration(bool is_duplicate_registration);
  void OnRegisterResponseReceived(
      bool is_duplicate_registration,
      base::Optional<std::vector<uint8_t>> device_response);
  void OnCheckForExcludedKeyHandle(
      ExcludeListIterator it,
      base::Optional<std::vector<uint8_t>> device_response);

  base::WeakPtrFactory<U2fRegisterOperation> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fRegisterOperation);
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_REGISTER_OPERATION_H_
