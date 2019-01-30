// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
#define CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/browser/authenticator_request_client_delegate.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

class AuthenticatorRequestDialogModel;

class ChromeAuthenticatorRequestDelegate
    : public content::AuthenticatorRequestClientDelegate,
      public AuthenticatorRequestDialogModel::Observer {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // The |render_frame_host| must outlive this instance.
  explicit ChromeAuthenticatorRequestDelegate(
      content::RenderFrameHost* render_frame_host);
  ~ChromeAuthenticatorRequestDelegate() override;

  base::WeakPtr<ChromeAuthenticatorRequestDelegate> AsWeakPtr();

#if defined(OS_MACOSX)
  std::string TouchIdAuthenticatorKeychainAccessGroup() override;
  std::string TouchIdMetadataSecret() override;
#endif

 private:
  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }
  content::BrowserContext* browser_context() const;

  // content::AuthenticatorRequestClientDelegate:
  void DidStartRequest() override;
  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override;
  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      base::OnceCallback<void(bool)> callback) override;
  bool IsFocused() override;

  // AuthenticatorRequestDialogModel::Observer:
  void OnModelDestroyed() override;

  content::RenderFrameHost* const render_frame_host_;
  AuthenticatorRequestDialogModel* weak_dialog_model_ = nullptr;

  base::WeakPtrFactory<ChromeAuthenticatorRequestDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAuthenticatorRequestDelegate);
};

#endif  // CHROME_BROWSER_WEBAUTHN_CHROME_AUTHENTICATOR_REQUEST_DELEGATE_H_
