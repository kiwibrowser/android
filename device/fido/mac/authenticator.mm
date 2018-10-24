// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/authenticator.h"

#import <LocalAuthentication/LocalAuthentication.h>

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/mac/get_assertion_operation.h"
#include "device/fido/mac/make_credential_operation.h"
#include "device/fido/mac/util.h"

namespace device {
namespace fido {
namespace mac {

// static
// NOTE(martinkr): This is currently only called from |CreateIfAvailable| but
// will also be needed for the implementation of
// IsUserVerifyingPlatformAuthenticatorAvailable() (see
// https://www.w3.org/TR/webauthn/#isUserVerifyingPlatformAuthenticatorAvailable).
bool TouchIdAuthenticator::IsAvailable() {
  base::scoped_nsobject<LAContext> context([[LAContext alloc] init]);
  return
      [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                           error:nil];
}

// static
std::unique_ptr<TouchIdAuthenticator> TouchIdAuthenticator::CreateIfAvailable(
    std::string keychain_access_group,
    std::string metadata_secret) {
  return IsAvailable() ? base::WrapUnique(new TouchIdAuthenticator(
                             std::move(keychain_access_group),
                             std::move(metadata_secret)))
                       : nullptr;
}

TouchIdAuthenticator::~TouchIdAuthenticator() = default;

void TouchIdAuthenticator::MakeCredential(
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    CtapMakeCredentialRequest request,
    MakeCredentialCallback callback) {
  DCHECK(!operation_);
  operation_ = std::make_unique<MakeCredentialOperation>(
      std::move(request), metadata_secret_, keychain_access_group_,
      std::move(callback));
  operation_->Run();
}

void TouchIdAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        GetAssertionCallback callback) {
  DCHECK(!operation_);
  operation_ = std::make_unique<GetAssertionOperation>(
      std::move(request), metadata_secret_, keychain_access_group_,
      std::move(callback));
  operation_->Run();
}

void TouchIdAuthenticator::Cancel() {
  // If there is an operation pending, delete it, which will clean up any
  // pending callbacks, e.g. if the operation is waiting for a response from
  // the Touch ID prompt. Note that we cannot cancel the actual prompt once it
  // has been shown.
  operation_.reset();
}

std::string TouchIdAuthenticator::GetId() const {
  return "TouchIdAuthenticator";
}

TouchIdAuthenticator::TouchIdAuthenticator(std::string keychain_access_group,
                                           std::string metadata_secret)
    : keychain_access_group_(std::move(keychain_access_group)),
      metadata_secret_(std::move(metadata_secret)) {}

}  // namespace mac
}  // namespace fido
}  // namespace device
