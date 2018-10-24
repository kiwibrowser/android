// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_

#include <string>

#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

class CookieSettingsBase {
 public:
  CookieSettingsBase() = default;
  virtual ~CookieSettingsBase() = default;

  // Returns true if the cookie associated with |domain| should be deleted
  // on exit.
  // This uses domain matching as described in section 5.1.3 of RFC 6265 to
  // identify content setting rules that could have influenced the cookie
  // when it was created.
  // As |cookie_settings| can be expensive to create, it should be cached if
  // multiple calls to ShouldDeleteCookieOnExit() are made.
  //
  // This may be called on any thread.
  bool ShouldDeleteCookieOnExit(
      const ContentSettingsForOneType& cookie_settings,
      const std::string& domain,
      bool is_https) const;

  // Returns true if the page identified by (|url|, |first_party_url|) is
  // allowed to access (i.e., read or write) cookies.
  //
  // This may be called on any thread.
  bool IsCookieAccessAllowed(const GURL& url,
                             const GURL& first_party_url) const;

  // Returns true if the cookie set by a page identified by |url| should be
  // session only. Querying this only makes sense if |IsCookieAccessAllowed|
  // has returned true.
  //
  // This may be called on any thread.
  bool IsCookieSessionOnly(const GURL& url) const;

  // A helper for applying third party cookie blocking rules.
  virtual void GetCookieSetting(const GURL& url,
                                const GURL& first_party_url,
                                content_settings::SettingSource* source,
                                ContentSetting* cookie_setting) const = 0;

  // Determines whether |setting| is a valid content setting for cookies.
  static bool IsValidSetting(ContentSetting setting);
  // Determines whether |setting| means the cookie should be allowed.
  static bool IsAllowed(ContentSetting setting);

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieSettingsBase);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_COOKIE_SETTINGS_BASE_H_
