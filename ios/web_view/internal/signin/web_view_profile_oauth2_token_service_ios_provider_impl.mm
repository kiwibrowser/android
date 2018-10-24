// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_profile_oauth2_token_service_ios_provider_impl.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web_view/internal/signin/ios_web_view_signin_client.h"
#import "ios/web_view/public/cwv_identity.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebViewProfileOAuth2TokenServiceIOSProviderImpl::
    WebViewProfileOAuth2TokenServiceIOSProviderImpl() {}

WebViewProfileOAuth2TokenServiceIOSProviderImpl::
    ~WebViewProfileOAuth2TokenServiceIOSProviderImpl() = default;

void WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    const AccessTokenCallback& callback) {
}

std::vector<ProfileOAuth2TokenServiceIOSProvider::AccountInfo>
WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAllAccounts() const {
  return {};
}

AuthenticationErrorCategory
WebViewProfileOAuth2TokenServiceIOSProviderImpl::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  // TODO(crbug.com/780937): Implement fully.
  return kAuthenticationErrorCategoryUnknownErrors;
}
