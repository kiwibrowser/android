// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/base/models/menu_model.h"

// Browser Test for AppListClientImpl.
using AppListClientImplBrowserTest = extensions::PlatformAppBrowserTest;

// Test AppListClient::IsAppOpen for extension apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, IsExtensionAppOpen) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  EXPECT_FALSE(delegate->IsAppOpen("fake_extension_app_id"));

  base::FilePath extension_path = test_data_dir_.AppendASCII("app");
  const extensions::Extension* extension_app = LoadExtension(extension_path);
  ASSERT_NE(nullptr, extension_app);
  EXPECT_FALSE(delegate->IsAppOpen(extension_app->id()));
  {
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenApplication(AppLaunchParams(
        profile(), extension_app, extensions::LAUNCH_CONTAINER_WINDOW,
        WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate->IsAppOpen(extension_app->id()));
}

// Test AppListClient::IsAppOpen for platform apps.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, IsPlatformAppOpen) {
  AppListControllerDelegate* delegate = AppListClientImpl::GetInstance();
  EXPECT_FALSE(delegate->IsAppOpen("fake_platform_app_id"));

  const extensions::Extension* app = InstallPlatformApp("minimal");
  EXPECT_FALSE(delegate->IsAppOpen(app->id()));
  {
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    LaunchPlatformApp(app);
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate->IsAppOpen(app->id()));
}

// Test the CreateNewWindow function of the controller delegate.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, CreateNewWindow) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  AppListControllerDelegate* controller = client;
  ASSERT_TRUE(controller);

  EXPECT_EQ(1U, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(0U, chrome::GetBrowserCount(
                    browser()->profile()->GetOffTheRecordProfile()));

  controller->CreateNewWindow(browser()->profile(), false);
  EXPECT_EQ(2U, chrome::GetBrowserCount(browser()->profile()));

  controller->CreateNewWindow(browser()->profile(), true);
  EXPECT_EQ(1U, chrome::GetBrowserCount(
                    browser()->profile()->GetOffTheRecordProfile()));
}

// Test that all the items in the context menu for a hosted app have valid
// labels.
IN_PROC_BROWSER_TEST_F(AppListClientImplBrowserTest, ShowContextMenu) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client);

  // Show the app list to ensure it has loaded a profile.
  client->ShowAppList();
  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  EXPECT_TRUE(model_updater);

  // Get the webstore hosted app, which is always present.
  ChromeAppListItem* item = model_updater->FindItem(extensions::kWebStoreAppId);
  EXPECT_TRUE(item);

  base::RunLoop run_loop;
  std::unique_ptr<ui::MenuModel> menu_model;
  item->GetContextMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::MenuModel> created_menu) {
        menu_model = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(menu_model);

  int num_items = menu_model->GetItemCount();
  EXPECT_LT(0, num_items);

  for (int i = 0; i < num_items; i++) {
    if (menu_model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR)
      continue;

    base::string16 label = menu_model->GetLabelAt(i);
    EXPECT_FALSE(label.empty());
  }
}

// Browser Test for AppListClient that observes search result changes.
using AppListClientSearchResultsBrowserTest = extensions::ExtensionBrowserTest;

// Test showing search results, and uninstalling one of them while displayed.
IN_PROC_BROWSER_TEST_F(AppListClientSearchResultsBrowserTest,
                       UninstallSearchResult) {
  base::FilePath test_extension_path;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_extension_path));
  test_extension_path = test_extension_path.AppendASCII("extensions")
                            .AppendASCII("platform_apps")
                            .AppendASCII("minimal");

  AppListClientImpl* client = AppListClientImpl::GetInstance();
  ASSERT_TRUE(client);
  // Associate |client| with the current profile.
  client->UpdateProfile();

  AppListModelUpdater* model_updater = test::GetModelUpdater(client);
  ASSERT_TRUE(model_updater);
  app_list::SearchController* search_controller =
      client->GetSearchControllerForTest();
  ASSERT_TRUE(search_controller);

  // Install the extension.
  const extensions::Extension* extension = InstallExtension(
      test_extension_path, 1 /* expected_change: new install */);
  ASSERT_TRUE(extension);

  const std::string title = extension->name();

  // Show the app list first, otherwise we won't have a search box to update.
  client->ShowAppList();
  client->FlushMojoForTesting();

  // Currently the search box is empty, so we have no result.
  EXPECT_FALSE(search_controller->GetResultByTitleForTest(title));

  // Now a search finds the extension.
  model_updater->UpdateSearchBox(base::ASCIIToUTF16(title),
                                 true /* initiated_by_user */);

  // Ensure everything is done, from Chrome to Ash and backwards.
  client->FlushMojoForTesting();
  EXPECT_TRUE(search_controller->GetResultByTitleForTest(title));

  // Uninstall the extension.
  UninstallExtension(extension->id());

  // Ensure everything is done, from Chrome to Ash and backwards.
  client->FlushMojoForTesting();

  // We cannot find the extension any more.
  EXPECT_FALSE(search_controller->GetResultByTitleForTest(title));

  client->DismissView();
}

class AppListClientGuestModeBrowserTest : public InProcessBrowserTest {
 public:
  AppListClientGuestModeBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListClientGuestModeBrowserTest);
};

void AppListClientGuestModeBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                  user_manager::kGuestUserName);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                  TestingProfile::kTestUserProfileDir);
  command_line->AppendSwitch(switches::kIncognito);
}

// Test creating the initial app list in guest mode.
IN_PROC_BROWSER_TEST_F(AppListClientGuestModeBrowserTest, Incognito) {
  AppListClientImpl* client = AppListClientImpl::GetInstance();
  EXPECT_TRUE(client->GetCurrentAppListProfile());

  client->ShowAppList();
  EXPECT_EQ(browser()->profile(), client->GetCurrentAppListProfile());
}
