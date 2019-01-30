// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"

#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_client_impl.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"
#include "chrome/browser/ui/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/window/dialog_client_view.h"

// ChromeBrowserMainExtraParts used to install a MockNetworkChangeNotifier.
class ChromeBrowserMainExtraPartsNetFactoryInstaller
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsNetFactoryInstaller() = default;
  ~ChromeBrowserMainExtraPartsNetFactoryInstaller() override {
    // |network_change_notifier_| needs to be destroyed before |net_installer_|.
    network_change_notifier_.reset();
  }

  net::test::MockNetworkChangeNotifier* network_change_notifier() {
    return network_change_notifier_.get();
  }

  // ChromeBrowserMainExtraParts:
  void PreEarlyInitialization() override {}
  void PostMainMessageLoopStart() override {
    ASSERT_TRUE(net::NetworkChangeNotifier::HasNetworkChangeNotifier());
    net_installer_ =
        std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
    network_change_notifier_ =
        std::make_unique<net::test::MockNetworkChangeNotifier>();
    network_change_notifier_->SetConnectionType(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }

 private:
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      network_change_notifier_;
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest> net_installer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsNetFactoryInstaller);
};

class CrostiniInstallerViewBrowserTest : public DialogBrowserTest {
 public:
  class WaitingFakeConciergeClient : public chromeos::FakeConciergeClient {
   public:
    void StartTerminaVm(
        const vm_tools::concierge::StartVmRequest& request,
        chromeos::DBusMethodCallback<vm_tools::concierge::StartVmResponse>
            callback) override {
      chromeos::FakeConciergeClient::StartTerminaVm(request,
                                                    std::move(callback));
      if (closure_) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                      std::move(closure_));
      }
    }

    void WaitForStartTerminaVmCalled() {
      base::RunLoop loop;
      closure_ = loop.QuitClosure();
      loop.Run();
      EXPECT_TRUE(start_termina_vm_called());
    }

   private:
    base::OnceClosure closure_;
  };

  CrostiniInstallerViewBrowserTest()
      : waiting_fake_concierge_client_(new WaitingFakeConciergeClient()) {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetConciergeClient(
        base::WrapUnique(waiting_fake_concierge_client_));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowCrostiniInstallerView(browser()->profile(),
                              CrostiniUISurface::kSettings);
  }

  // BrowserTestBase:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    ChromeBrowserMainParts* chrome_browser_main_parts =
        static_cast<ChromeBrowserMainParts*>(browser_main_parts);
    extra_parts_ = new ChromeBrowserMainExtraPartsNetFactoryInstaller();
    chrome_browser_main_parts->AddParts(extra_parts_);
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kExperimentalCrostiniUI);
    DialogBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    browser()->profile()->GetPrefs()->SetBoolean(
        crostini::prefs::kCrostiniEnabled, true);
  }

  CrostiniInstallerView* ActiveView() {
    return CrostiniInstallerView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return ActiveView()->GetDialogClientView()->cancel_button() != nullptr;
  }

 protected:
  // Owned by chromeos::DBusThreadManager
  WaitingFakeConciergeClient* waiting_fake_concierge_client_ = nullptr;
  // Owned by content::Browser
  ChromeBrowserMainExtraPartsNetFactoryInstaller* extra_parts_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerViewBrowserTest);
};

// Test the dialog is actually launched from the app launcher.
IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InstallFlow) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  waiting_fake_concierge_client_->WaitForStartTerminaVmCalled();

  // RunUntilIdle in this case will run the rest of the install steps including
  // launching the terminal, on the UI thread.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectBucketCount(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kSuccess),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, InstallFlow_Offline) {
  base::HistogramTester histogram_tester;
  extra_parts_->network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  EXPECT_EQ(ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL,
            ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  EXPECT_FALSE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectBucketCount(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kErrorOffline),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, Cancel) {
  base::HistogramTester histogram_tester;

  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectBucketCount(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kNotStarted),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniInstallerViewBrowserTest, ErrorThenCancel) {
  base::HistogramTester histogram_tester;
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());
  vm_tools::concierge::StartVmResponse response;
  response.set_success(false);
  waiting_fake_concierge_client_->set_start_vm_response(std::move(response));

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_FALSE(ActiveView()->GetWidget()->IsClosed());
  waiting_fake_concierge_client_->WaitForStartTerminaVmCalled();
  ActiveView()->GetDialogClientView()->CancelWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, ActiveView());

  histogram_tester.ExpectBucketCount(
      "Crostini.SetupResult",
      static_cast<base::HistogramBase::Sample>(
          CrostiniInstallerView::SetupResult::kErrorStartingTermina),
      1);
}
