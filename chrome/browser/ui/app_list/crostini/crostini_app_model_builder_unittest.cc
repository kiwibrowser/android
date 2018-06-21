// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

using crostini::CrostiniTestHelper;

namespace {

std::vector<ChromeAppListItem*> GetAppListItems(
    AppListModelUpdater* model_updater) {
  std::vector<ChromeAppListItem*> result;
  for (size_t i = 0; i < model_updater->ItemCount(); ++i)
    result.push_back(model_updater->ItemAtForTest(i));
  return result;
}

std::vector<std::string> GetAppIds(AppListModelUpdater* model_updater) {
  std::vector<std::string> result;
  for (ChromeAppListItem* item : GetAppListItems(model_updater))
    result.push_back(item->id());
  return result;
}

std::vector<std::string> GetAppNames(AppListModelUpdater* model_updater) {
  std::vector<std::string> result;
  for (ChromeAppListItem* item : GetAppListItems(model_updater))
    result.push_back(item->name());
  return result;
}

}  // namespace

class CrostiniAppModelBuilderTest : public AppListTestBase {
 public:
  CrostiniAppModelBuilderTest() {}
  ~CrostiniAppModelBuilderTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalCrostiniUI);
    AppListTestBase::SetUp();
    CreateBuilder();
  }

  void TearDown() override {
    ResetBuilder();
    AppListTestBase::TearDown();
  }

 protected:
  void CreateBuilder() {
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<CrostiniAppModelBuilder>(controller_.get());
    builder_->Initialize(nullptr, profile_.get(), model_updater_.get());
  }

  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    model_updater_.reset();
  }

  crostini::CrostiniRegistryService* RegistryService() {
    return crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  }

  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<CrostiniAppModelBuilder> builder_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppModelBuilderTest);
};

// Test that the Terminal app is only shown when Crostini is enabled
TEST_F(CrostiniAppModelBuilderTest, EnableCrostini) {
  EXPECT_EQ(0u, model_updater_->ItemCount());
  CrostiniTestHelper::EnableCrostini(profile());
  EXPECT_EQ(1u, model_updater_->ItemCount());
  ChromeAppListItem* item = model_updater_->ItemAtForTest(0);
  EXPECT_EQ(kCrostiniTerminalId, item->id());
  EXPECT_EQ(kCrostiniTerminalAppName, item->name());
}

TEST_F(CrostiniAppModelBuilderTest, AppInstallation) {
  CrostiniTestHelper test_helper(profile());
  EXPECT_EQ(1u, model_updater_->ItemCount());

  test_helper.SetupDummyApps();
  EXPECT_THAT(GetAppIds(model_updater_.get()),
              testing::UnorderedElementsAreArray(
                  RegistryService()->GetRegisteredAppIds()));
  EXPECT_THAT(GetAppNames(model_updater_.get()),
              testing::UnorderedElementsAre(kCrostiniTerminalAppName, "dummy1",
                                            "dummy2"));

  test_helper.AddApp(CrostiniTestHelper::BasicApp("banana", "banana app name"));
  EXPECT_THAT(GetAppIds(model_updater_.get()),
              testing::UnorderedElementsAreArray(
                  RegistryService()->GetRegisteredAppIds()));
  EXPECT_THAT(GetAppNames(model_updater_.get()),
              testing::UnorderedElementsAre(kCrostiniTerminalAppName, "dummy1",
                                            "dummy2", "banana app name"));
}

// Test that the app model builder correctly picks up changes to existing apps.
TEST_F(CrostiniAppModelBuilderTest, UpdateApps) {
  CrostiniTestHelper test_helper(profile());
  test_helper.SetupDummyApps();
  EXPECT_EQ(3u, model_updater_->ItemCount());

  // Setting NoDisplay to true should hide an app.
  vm_tools::apps::App dummy1 = test_helper.GetApp(0);
  dummy1.set_no_display(true);
  test_helper.AddApp(dummy1);
  EXPECT_EQ(2u, model_updater_->ItemCount());
  EXPECT_THAT(
      GetAppIds(model_updater_.get()),
      testing::UnorderedElementsAre(
          kCrostiniTerminalId, CrostiniTestHelper::GenerateAppId("dummy2")));

  // Setting NoDisplay to false should unhide an app.
  dummy1.set_no_display(false);
  test_helper.AddApp(dummy1);
  EXPECT_EQ(3u, model_updater_->ItemCount());
  EXPECT_THAT(GetAppIds(model_updater_.get()),
              testing::UnorderedElementsAreArray(
                  RegistryService()->GetRegisteredAppIds()));

  // Changes to app names should be detected.
  vm_tools::apps::App dummy2 =
      CrostiniTestHelper::BasicApp("dummy2", "new name");
  test_helper.AddApp(dummy2);
  EXPECT_EQ(3u, model_updater_->ItemCount());
  EXPECT_THAT(GetAppIds(model_updater_.get()),
              testing::UnorderedElementsAreArray(
                  RegistryService()->GetRegisteredAppIds()));
  EXPECT_THAT(GetAppNames(model_updater_.get()),
              testing::UnorderedElementsAre(kCrostiniTerminalAppName, "dummy1",
                                            "new name"));
}

// Test that the app model builder handles removed apps
TEST_F(CrostiniAppModelBuilderTest, RemoveApps) {
  CrostiniTestHelper test_helper(profile());
  test_helper.SetupDummyApps();
  EXPECT_EQ(3u, model_updater_->ItemCount());

  // Remove dummy1
  test_helper.RemoveApp(0);
  EXPECT_EQ(2u, model_updater_->ItemCount());

  // Remove dummy2
  test_helper.RemoveApp(0);
  EXPECT_EQ(1u, model_updater_->ItemCount());
}

// Test that the Terminal app is removed when Crostini is disabled.
TEST_F(CrostiniAppModelBuilderTest, DisableCrostini) {
  CrostiniTestHelper test_helper(profile());
  test_helper.SetupDummyApps();
  EXPECT_EQ(3u, model_updater_->ItemCount());

  // The uninstall flow removes all apps before setting the CrostiniEnabled pref
  // to false, so we need to do that explicitly too.
  RegistryService()->ClearApplicationList(kCrostiniDefaultVmName,
                                          kCrostiniDefaultContainerName);
  CrostiniTestHelper::DisableCrostini(profile());
  EXPECT_EQ(0u, model_updater_->ItemCount());
}
