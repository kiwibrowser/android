// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingPrefsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutReportingEnabled, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingScoutGroupSelected, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingSawInterstitialExtendedReporting, false);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingSawInterstitialScoutReporting, false);
    prefs_.registry()->RegisterStringPref(
        prefs::kPasswordProtectionChangePasswordURL, "");
    prefs_.registry()->RegisterListPref(prefs::kPasswordProtectionLoginURLs);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
    prefs_.registry()->RegisterListPref(prefs::kSafeBrowsingWhitelistDomains);

    ResetExperiments(/*can_show_scout=*/false);
  }

  void ResetPrefs(bool scout_reporting, bool scout_group) {
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutReportingEnabled,
                      scout_reporting);
    prefs_.SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, scout_group);
  }

  void ResetExperiments(bool can_show_scout) {
    std::vector<base::StringPiece> enabled_features;
    std::vector<base::StringPiece> disabled_features;

    auto* target_vector =
        can_show_scout ? &enabled_features : &disabled_features;
    target_vector->push_back(kCanShowScoutOptIn.name);

    feature_list_.reset(new base::test::ScopedFeatureList);
    feature_list_->InitFromCommandLine(
        base::JoinString(enabled_features, ","),
        base::JoinString(disabled_features, ","));
  }

  void EnableEnterprisePasswordProtectionFeature() {
    feature_list_.reset(new base::test::ScopedFeatureList);
    feature_list_->InitAndEnableFeature(kEnterprisePasswordProtectionV1);
  }

  std::string GetActivePref() { return GetExtendedReportingPrefName(prefs_); }

  // Convenience method for explicitly setting up all combinations of prefs and
  // experiments.
  void TestGetPrefName(bool scout_reporting,
                       bool scout_group,
                       bool can_show_scout,
                       const std::string& expected_pref) {
    ResetPrefs(scout_reporting, scout_group);
    ResetExperiments(can_show_scout);
    EXPECT_EQ(expected_pref, GetActivePref())
        << " scout=" << scout_reporting << " scout_group=" << scout_group
        << " can_show_scout=" << can_show_scout;
  }

  bool IsScoutGroupSelected() {
    return prefs_.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected);
  }

  void ExpectPrefs(bool scout_reporting, bool scout_group) {
    LOG(INFO) << "Pref values: scout=" << scout_reporting
              << " scout_group=" << scout_group;
    EXPECT_EQ(scout_reporting,
              prefs_.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
    EXPECT_EQ(scout_group,
              prefs_.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected));
  }

  void ExpectPrefsExist(bool scout_reporting, bool scout_group) {
    LOG(INFO) << "Prefs exist: scout=" << scout_reporting
              << " scout_group=" << scout_group;
    EXPECT_EQ(scout_reporting,
              prefs_.HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled));
    EXPECT_EQ(scout_group,
              prefs_.HasPrefPath(prefs::kSafeBrowsingScoutGroupSelected));
  }
  TestingPrefServiceSimple prefs_;

 private:
  std::unique_ptr<base::test::ScopedFeatureList> feature_list_;
  content::TestBrowserThreadBundle thread_bundle_;
};

// This test ensures that we correctly select Scout as the
// active preference in a number of common scenarios.
TEST_F(SafeBrowsingPrefsTest, GetExtendedReportingPrefName_Common) {
  const std::string& scout = prefs::kSafeBrowsingScoutReportingEnabled;

  // By default (all prefs and experiment features disabled), Scout pref is
  // used.
  TestGetPrefName(false, false, false, scout);

  // Changing any prefs (including ScoutGroupSelected) keeps Scout as the active
  // pref because the experiment remains in the Control group.
  TestGetPrefName(/*scout=*/true, false, false, scout);
  TestGetPrefName(false, /*scout_group=*/true, false, scout);

  // Being in the experiment group with ScoutGroup selected makes Scout the
  // active pref.
  TestGetPrefName(false, /*scout_group=*/true, /*can_show_scout=*/true, scout);

  // When ScoutGroup is not selected then Scout still remains the active pref,
  // regardless if the experiment is enabled.
  TestGetPrefName(false, false, /*can_show_scout=*/true, scout);
}

// Here we exhaustively check all combinations of pref and experiment states.
// This should help catch regressions.
TEST_F(SafeBrowsingPrefsTest, GetExtendedReportingPrefName_Exhaustive) {
  const std::string& scout = prefs::kSafeBrowsingScoutReportingEnabled;
  TestGetPrefName(false, false, false, scout);
  TestGetPrefName(false, false, true, scout);
  TestGetPrefName(false, true, false, scout);
  TestGetPrefName(false, true, true, scout);
  TestGetPrefName(true, false, false, scout);
  TestGetPrefName(true, false, true, scout);
  TestGetPrefName(true, true, false, scout);
  TestGetPrefName(true, true, true, scout);
}

TEST_F(SafeBrowsingPrefsTest, ChooseOptInText) {
  // Ensure that Scout resources are always chosen.
  const int kSberResource = 100;
  const int kScoutResource = 500;

  // By default, Scout opt-in is used
  EXPECT_EQ(kScoutResource,
            ChooseOptInTextResource(prefs_, kSberResource, kScoutResource));

  // Enabling Scout still uses the Scout opt-in text.
  ResetExperiments(/*can_show_scout=*/true);
  ResetPrefs(/*scout=*/false, /*scout_group=*/true);
  EXPECT_EQ(kScoutResource,
            ChooseOptInTextResource(prefs_, kSberResource, kScoutResource));
}

TEST_F(SafeBrowsingPrefsTest, GetSafeBrowsingExtendedReportingLevel) {
  // By Default, extneded reporting is off.
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));

  // The value of the Scout pref affects the reporting level directly,
  // regardless of the experiment configuration since Scout is the only level
  // we are using.
  // No scout group.
  ResetPrefs(/*scout_reporting=*/true, /*scout_group=*/false);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
  // Scout group but no experiment.
  ResetPrefs(/*scout_reporting=*/true, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
  ResetExperiments(/*can_show_scout=*/true);
  // Scout pref off, so reporting is off.
  ResetPrefs(/*scout_reporting=*/false, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));
  // Scout pref off with the experiment group off, so reporting remains off.
  ResetPrefs(/*scout_reporting=*/false, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_OFF, GetExtendedReportingLevel(prefs_));
  // Turning on Scout gives us Scout level reporting
  ResetPrefs(/*scout_reporting=*/true, /*scout_group=*/true);
  EXPECT_EQ(SBER_LEVEL_SCOUT, GetExtendedReportingLevel(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyMatchesPasswordProtectionLoginURL) {
  EnableEnterprisePasswordProtectionFeature();

  GURL url("https://mydomain.com/login.html#ref?username=alice");
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_FALSE(MatchesPasswordProtectionLoginURL(url, prefs_));

  base::ListValue login_urls;
  login_urls.AppendString("https://otherdomain.com/login.html");
  prefs_.Set(prefs::kPasswordProtectionLoginURLs, login_urls);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_FALSE(MatchesPasswordProtectionLoginURL(url, prefs_));

  login_urls.AppendString("https://mydomain.com/login.html");
  prefs_.Set(prefs::kPasswordProtectionLoginURLs, login_urls);
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionLoginURLs));
  EXPECT_TRUE(MatchesPasswordProtectionLoginURL(url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest,
       VerifyMatchesPasswordProtectionChangePasswordURL) {
  EnableEnterprisePasswordProtectionFeature();

  GURL url("https://mydomain.com/change_password.html#ref?username=alice");
  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_FALSE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));

  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://otherdomain.com/change_password.html");
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_FALSE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));

  prefs_.SetString(prefs::kPasswordProtectionChangePasswordURL,
                   "https://mydomain.com/change_password.html");
  EXPECT_TRUE(prefs_.HasPrefPath(prefs::kPasswordProtectionChangePasswordURL));
  EXPECT_TRUE(MatchesPasswordProtectionChangePasswordURL(url, prefs_));
}

TEST_F(SafeBrowsingPrefsTest, IsExtendedReportingPolicyManaged) {
  // This test checks that manipulating SBEROptInAllowed and the management
  // state of SBER behaves as expected. Below, we describe what should happen
  // to the results of IsExtendedReportingPolicyManaged and
  // IsExtendedReportingOptInAllowed.

  // Confirm default state, SBER should be disabled, OptInAllowed should
  // be enabled, and SBER is not managed.
  EXPECT_FALSE(IsExtendedReportingEnabled(prefs_));
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));

  // Setting SBEROptInAllowed to false disallows opt-in but doesn't change
  // whether SBER is managed.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, false);
  EXPECT_FALSE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));
  // Setting the value back to true reverts back to the default.
  prefs_.SetBoolean(prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
  EXPECT_FALSE(IsExtendedReportingPolicyManaged(prefs_));

  // Make the SBER pref managed and enable it and ensure that the pref gets
  // the expected value. Making SBER managed doesn't change the
  // SBEROptInAllowed setting.
  prefs_.SetManagedPref(GetExtendedReportingPrefName(prefs_),
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(prefs_.IsManagedPreference(GetExtendedReportingPrefName(prefs_)));
  // The value of the pref comes from the policy.
  EXPECT_TRUE(IsExtendedReportingEnabled(prefs_));
  // SBER being managed doesn't change the SBEROptInAllowed pref.
  EXPECT_TRUE(IsExtendedReportingOptInAllowed(prefs_));
}

TEST_F(SafeBrowsingPrefsTest, VerifyIsURLWhitelistedByPolicy) {
  GURL target_url("https://www.foo.com");
  // When PrefMember is null, URL is not whitelisted.
  EXPECT_FALSE(IsURLWhitelistedByPolicy(target_url, nullptr));

  EXPECT_FALSE(prefs_.HasPrefPath(prefs::kSafeBrowsingWhitelistDomains));
  base::ListValue whitelisted_domains;
  whitelisted_domains.AppendString("foo.com");
  prefs_.Set(prefs::kSafeBrowsingWhitelistDomains, whitelisted_domains);
  StringListPrefMember string_list_pref;
  string_list_pref.Init(prefs::kSafeBrowsingWhitelistDomains, &prefs_);
  EXPECT_TRUE(IsURLWhitelistedByPolicy(target_url, prefs_));
  EXPECT_TRUE(IsURLWhitelistedByPolicy(target_url, &string_list_pref));

  GURL not_whitelisted_url("https://www.bar.com");
  EXPECT_FALSE(IsURLWhitelistedByPolicy(not_whitelisted_url, prefs_));
  EXPECT_FALSE(
      IsURLWhitelistedByPolicy(not_whitelisted_url, &string_list_pref));
}
}  // namespace safe_browsing
