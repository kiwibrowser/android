// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/public_key_credential.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/credentialmanager/credential_manager_proxy.h"
#include "third_party/blink/renderer/modules/credentialmanager/scoped_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

namespace {
// https://www.w3.org/TR/webauthn/#dom-publickeycredential-type-slot:
constexpr char kPublicKeyCredentialType[] = "public-key";

void OnIsUserVerifyingComplete(
    std::unique_ptr<ScopedPromiseResolver> scoped_resolver,
    bool available) {
  scoped_resolver->Release()->Resolve(available);
}
}  // namespace

PublicKeyCredential* PublicKeyCredential::Create(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    const AuthenticationExtensionsClientOutputs& extension_outputs) {
  return new PublicKeyCredential(id, raw_id, response, extension_outputs);
}

PublicKeyCredential::PublicKeyCredential(
    const String& id,
    DOMArrayBuffer* raw_id,
    AuthenticatorResponse* response,
    const AuthenticationExtensionsClientOutputs& extension_outputs)
    : Credential(id, kPublicKeyCredentialType),
      raw_id_(raw_id),
      response_(response),
      extension_outputs_(extension_outputs) {}

ScriptPromise
PublicKeyCredential::isUserVerifyingPlatformAuthenticatorAvailable(
    ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  DCHECK(resolver->GetExecutionContext());
  if (resolver->GetExecutionContext()->IsContextDestroyed()) {
    resolver->Reject();
    return promise;
  }

  auto* authenticator =
      CredentialManagerProxy::From(script_state)->Authenticator();
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(WTF::Bind(
      &OnIsUserVerifyingComplete,
      WTF::Passed(std::make_unique<ScopedPromiseResolver>(resolver))));
  return promise;
}

void PublicKeyCredential::getClientExtensionResults(
    AuthenticationExtensionsClientOutputs& result) const {
  result = extension_outputs_;
}

void PublicKeyCredential::Trace(blink::Visitor* visitor) {
  visitor->Trace(raw_id_);
  visitor->Trace(response_);
  Credential::Trace(visitor);
}

bool PublicKeyCredential::IsPublicKeyCredential() const {
  return true;
}

}  // namespace blink
