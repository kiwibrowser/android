// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_preferences::TestingPrefServiceSyncable;
using AppInfo = web_app::WebAppPolicyManager::AppInfo;

namespace web_app {

namespace {

const char kUrl1[] = "https://mobile.twitter.com";
const char kUrl2[] = "https://www.google.com";

}  // namespace

class WebAppPolicyManagerTest : public testing::Test {
 public:
  class TestPendingAppManager;
};

class WebAppPolicyManagerTest::TestPendingAppManager
    : public WebAppPolicyManager::PendingAppManager {
 public:
  TestPendingAppManager() = default;
  ~TestPendingAppManager() override = default;

  void ProcessAppOperations(
      std::vector<WebAppPolicyManager::AppInfo> apps_to_install) override {
    last_apps_to_install_ = std::move(apps_to_install);
  }

  const std::vector<AppInfo>& last_apps_to_install() {
    return last_apps_to_install_;
  }

 private:
  std::vector<AppInfo> last_apps_to_install_;

  DISALLOW_COPY_AND_ASSIGN(TestPendingAppManager);
};

TEST_F(WebAppPolicyManagerTest, NoForceInstalledAppsPrefValue) {
  auto prefs = std::make_unique<TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  auto* pending_app_manager_ptr = pending_app_manager.get();

  WebAppPolicyManager web_app_policy_manager(prefs.get(),
                                             std::move(pending_app_manager));
  const auto& apps_to_install = pending_app_manager_ptr->last_apps_to_install();

  EXPECT_TRUE(apps_to_install.empty());
}

TEST_F(WebAppPolicyManagerTest, NoForceInstalledApps) {
  auto prefs = std::make_unique<TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());

  prefs->Set(prefs::kWebAppInstallForceList,
             base::Value(base::Value::Type::LIST));

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  auto* pending_app_manager_ptr = pending_app_manager.get();

  WebAppPolicyManager web_app_policy_manager(prefs.get(),
                                             std::move(pending_app_manager));
  const auto& apps_to_install = pending_app_manager_ptr->last_apps_to_install();

  EXPECT_TRUE(apps_to_install.empty());
}

TEST_F(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  auto prefs = std::make_unique<TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());

  base::Value list(base::Value::Type::LIST);
  // Add two sites, one that opens in a window and one that opens in a tab.
  {
    base::Value item1(base::Value::Type::DICTIONARY);
    item1.SetKey(kUrlKey, base::Value(kUrl1));
    item1.SetKey(kLaunchTypeKey, base::Value(kLaunchTypeWindowValue));

    base::Value item2(base::Value::Type::DICTIONARY);
    item2.SetKey(kUrlKey, base::Value(kUrl2));
    item2.SetKey(kLaunchTypeKey, base::Value(kLaunchTypeTabValue));

    list.GetList().push_back(std::move(item1));
    list.GetList().push_back(std::move(item2));

    prefs->Set(prefs::kWebAppInstallForceList, std::move(list));
  }

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  auto* pending_app_manager_ptr = pending_app_manager.get();

  WebAppPolicyManager web_app_policy_manager(prefs.get(),
                                             std::move(pending_app_manager));
  const auto& apps_to_install = pending_app_manager_ptr->last_apps_to_install();

  std::vector<AppInfo> expected_apps_to_install;
  expected_apps_to_install.emplace_back(
      GURL(kUrl1), WebAppPolicyManager::LaunchType::kWindow);
  expected_apps_to_install.emplace_back(GURL(kUrl2),
                                        WebAppPolicyManager::LaunchType::kTab);

  EXPECT_EQ(apps_to_install, expected_apps_to_install);
}

}  // namespace web_app
