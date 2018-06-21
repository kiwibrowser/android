// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

constexpr char kURL[] = "http://foo.com";
constexpr char kOtherURL[] = "http://other.com";

ContentSettingPatternSource CreateSetting(const std::string& url,
                                          const std::string& secondary_url,
                                          ContentSetting setting) {
  return ContentSettingPatternSource(
      ContentSettingsPattern::FromString(url),
      ContentSettingsPattern::FromString(secondary_url), base::Value(setting),
      std::string(), false);
}

TEST(CookieSettingsTest, GetCookieSettingDefault) {
  CookieSettings settings;
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST(CookieSettingsTest, GetCookieSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingMustMatchBothPatterns) {
  CookieSettings settings;
  // This setting needs kOtherURL as the secondary pattern.
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_BLOCK)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);

  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingGetsFirstSetting) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kURL, CONTENT_SETTING_BLOCK),
       CreateSetting(kURL, kURL, CONTENT_SETTING_SESSION_ONLY)});
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingDontBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(false);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST(CookieSettingsTest, GetCookieSettingBlockThirdParty) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_BLOCK);
}

TEST(CookieSettingsTest, GetCookieSettingDontBlockThirdPartyWithException) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting(kURL, kOtherURL, CONTENT_SETTING_ALLOW)});
  settings.set_block_third_party_cookies(true);
  ContentSetting setting;
  settings.GetCookieSetting(GURL(kURL), GURL(kOtherURL), nullptr, &setting);
  EXPECT_EQ(setting, CONTENT_SETTING_ALLOW);
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSettings) {
  CookieSettings settings;
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateNoSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate());
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateSessionOnly) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_TRUE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

TEST(CookieSettingsTest, CreateDeleteCookieOnExitPredicateAllow) {
  CookieSettings settings;
  settings.set_content_settings(
      {CreateSetting("*", "*", CONTENT_SETTING_ALLOW),
       CreateSetting("*", "*", CONTENT_SETTING_SESSION_ONLY)});
  EXPECT_FALSE(settings.CreateDeleteCookieOnExitPredicate().Run(kURL, false));
}

}  // namespace
}  // namespace network
