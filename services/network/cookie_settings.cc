// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"
#include "base/bind.h"
#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"

namespace network {
namespace {
bool IsDefaultSetting(const ContentSettingPatternSource& setting) {
  return setting.primary_pattern.MatchesAllHosts() &&
         setting.secondary_pattern.MatchesAllHosts();
}
}  // namespace

CookieSettings::CookieSettings() {}
CookieSettings::~CookieSettings() {}

SessionCleanupCookieStore::DeleteCookiePredicate
CookieSettings::CreateDeleteCookieOnExitPredicate() const {
  if (!HasSessionOnlyOrigins())
    return SessionCleanupCookieStore::DeleteCookiePredicate();
  return base::BindRepeating(&CookieSettings::ShouldDeleteCookieOnExit,
                             base::Unretained(this),
                             base::ConstRef(content_settings_));
}

void CookieSettings::GetCookieSetting(const GURL& url,
                                      const GURL& first_party_url,
                                      content_settings::SettingSource* source,
                                      ContentSetting* cookie_setting) const {
  // Default to allowing cookies.
  *cookie_setting = CONTENT_SETTING_ALLOW;
  bool block_third = block_third_party_cookies_;
  for (const auto& entry : content_settings_) {
    if (entry.primary_pattern.Matches(url) &&
        entry.secondary_pattern.Matches(first_party_url)) {
      *cookie_setting = entry.GetContentSetting();
      // Only continue to block third party cookies if there is not an explicit
      // exception.
      if (!IsDefaultSetting(entry))
        block_third = false;
      break;
    }
  }

  net::StaticCookiePolicy policy(
      net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);
  if (block_third && policy.CanAccessCookies(url, first_party_url) != net::OK)
    *cookie_setting = CONTENT_SETTING_BLOCK;
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  for (const auto& entry : content_settings_) {
    if (entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY)
      return true;
  }
  return false;
}

}  // namespace network
