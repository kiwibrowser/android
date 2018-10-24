// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/cookie_settings_base.h"
#include "net/cookies/cookie_util.h"
#include "url/gurl.h"

namespace content_settings {

bool CookieSettingsBase::ShouldDeleteCookieOnExit(
    const ContentSettingsForOneType& cookie_settings,
    const std::string& domain,
    bool is_https) const {
  GURL origin = net::cookie_util::CookieOriginToURL(domain, is_https);
  ContentSetting setting;
  GetCookieSetting(origin, origin, nullptr, &setting);
  DCHECK(IsValidSetting(setting));
  if (setting == CONTENT_SETTING_ALLOW)
    return false;
  // Non-secure cookies are readable by secure sites. We need to check for
  // https pattern if http is not allowed. The section below is independent
  // of the scheme so we can just retry from here.
  if (!is_https)
    return ShouldDeleteCookieOnExit(cookie_settings, domain, true);
  // Check if there is a more precise rule that "domain matches" this cookie.
  bool matches_session_only_rule = false;
  for (const auto& entry : cookie_settings) {
    const std::string& host = entry.primary_pattern.GetHost();
    if (net::cookie_util::IsDomainMatch(domain, host)) {
      if (entry.GetContentSetting() == CONTENT_SETTING_ALLOW) {
        return false;
      } else if (entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY) {
        matches_session_only_rule = true;
      }
    }
  }
  return setting == CONTENT_SETTING_SESSION_ONLY || matches_session_only_rule;
}

bool CookieSettingsBase::IsCookieAccessAllowed(
    const GURL& url,
    const GURL& first_party_url) const {
  ContentSetting setting;
  GetCookieSetting(url, first_party_url, nullptr, &setting);
  return IsAllowed(setting);
}

bool CookieSettingsBase::IsCookieSessionOnly(const GURL& origin) const {
  ContentSetting setting;
  GetCookieSetting(origin, origin, nullptr, &setting);
  DCHECK(IsValidSetting(setting));
  return setting == CONTENT_SETTING_SESSION_ONLY;
}

// static
bool CookieSettingsBase::IsValidSetting(ContentSetting setting) {
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY ||
          setting == CONTENT_SETTING_BLOCK);
}

// static
bool CookieSettingsBase::IsAllowed(ContentSetting setting) {
  DCHECK(IsValidSetting(setting));
  return (setting == CONTENT_SETTING_ALLOW ||
          setting == CONTENT_SETTING_SESSION_ONLY);
}

}  // namespace content_settings
