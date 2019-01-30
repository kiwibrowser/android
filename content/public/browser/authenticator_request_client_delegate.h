// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

// Interface that the embedder should implement to provide the //content layer
// with embedder-specific configuration for a single Web Authentication API [1]
// request serviced in a given RenderFrame.
//
// [1]: See https://www.w3.org/TR/webauthn/.
class CONTENT_EXPORT AuthenticatorRequestClientDelegate {
 public:
  AuthenticatorRequestClientDelegate();
  virtual ~AuthenticatorRequestClientDelegate();

  // Notifies the delegate that the request is actually starting.
  virtual void DidStartRequest();

  // Returns true if the given relying party ID is permitted to receive
  // individual attestation certificates. This:
  //  a) triggers a signal to the security key that returning individual
  //     attestation certificates is permitted, and
  //  b) skips any permission prompt for attestation.
  virtual bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id);

  // Invokes |callback| with |true| if the given relying party ID is permitted
  // to receive attestation certificates from a device. Otherwise invokes
  // |callback| with |false|.
  //
  // Since these certificates may uniquely identify the authenticator, the
  // embedder may choose to show a permissions prompt to the user, and only
  // invoke |callback| afterwards. This may hairpin |callback|.
  virtual void ShouldReturnAttestation(const std::string& relying_party_id,
                                       base::OnceCallback<void(bool)> callback);

  // Returns whether the WebContents corresponding to |render_frame_host| is the
  // active tab in the focused window. We do not want to allow
  // authenticatorMakeCredential operations to be triggered by background tabs.
  //
  // Note that the default implementation of this function, and the
  // implementation in ChromeContentBrowserClient for Android, return |true| so
  // that testing is possible.
  virtual bool IsFocused();

#if defined(OS_MACOSX)
  // Returns the kechain-access-group value used for WebAuthn credentials
  // stored in the macOS keychain by the built-in Touch ID authenticator. For
  // more information on this, refer to |device::fido::TouchIdAuthenticator|.
  // This method may to return empty string or some other placeholder value on
  // platforms where |TouchIdAuthenticator| is not used.
  virtual std::string TouchIdAuthenticatorKeychainAccessGroup();

  // Returns the secret used to derive key material when encrypting WebAuthn
  // credential metadata for storage in the macOS keychain. Chrome returns
  // different secrets for each user profile in order to logically separate
  // credentials per profile. This method may to return empty string or some
  // other placeholder value on platforms where |TouchIdAuthenticator| is not
  // used.
  virtual std::string TouchIdMetadataSecret();
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticatorRequestClientDelegate);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_REQUEST_CLIENT_DELEGATE_H_
