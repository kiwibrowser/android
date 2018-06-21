// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/authenticator_request_client_delegate.h"

#include "base/callback.h"
#include "base/strings/string_piece.h"

namespace content {

AuthenticatorRequestClientDelegate::AuthenticatorRequestClientDelegate() =
    default;
AuthenticatorRequestClientDelegate::~AuthenticatorRequestClientDelegate() =
    default;

void AuthenticatorRequestClientDelegate::DidStartRequest() {}

bool AuthenticatorRequestClientDelegate::ShouldPermitIndividualAttestation(
    const std::string& relying_party_id) {
  return false;
}

void AuthenticatorRequestClientDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool AuthenticatorRequestClientDelegate::IsFocused() {
  return true;
}

#if defined(OS_MACOSX)
std::string
AuthenticatorRequestClientDelegate::TouchIdAuthenticatorKeychainAccessGroup() {
  NOTREACHED();
  return std::string();
}
#endif

#if defined(OS_MACOSX)
std::string AuthenticatorRequestClientDelegate::TouchIdMetadataSecret() {
  NOTREACHED();
  return std::string();
}
#endif

}  // namespace content
