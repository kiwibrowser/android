// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/machine_level_user_cloud_policy_register_watcher.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/ui/enterprise_startup_dialog.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::_;
using RegisterResult =
    policy::MachineLevelUserCloudPolicyController::RegisterResult;

namespace policy {

namespace {

constexpr char kEnrollmentToken[] = "enrollment-token";
constexpr char kDMToken[] = "dm-token";
constexpr char kClientId[] = "client-id";

// A fake token storage class for tested code to get enrollment token and DM
// token.
class FakeDMTokenStorage : public BrowserDMTokenStorage {
 public:
  FakeDMTokenStorage() = default;
  std::string RetrieveDMToken() override { return dm_token_; }
  std::string RetrieveEnrollmentToken() override { return enrollment_token_; }
  std::string RetrieveClientId() override { return kClientId; }
  void StoreDMToken(const std::string& dm_token,
                    StoreCallback callback) override {
    NOTREACHED();
  }

  void set_dm_token(const std::string& dm_token) { dm_token_ = dm_token; }
  void set_enrollment_token(const std::string& enrollment_token) {
    enrollment_token_ = enrollment_token;
  }

 private:
  std::string enrollment_token_;
  std::string dm_token_;

  DISALLOW_COPY_AND_ASSIGN(FakeDMTokenStorage);
};

// A fake MachineLevelUserCloudPolicyController that notifies all observers the
// machine level user cloud policy enrollment process has been finished.
class FakeMachineLevelUserCloudPolicyController
    : public MachineLevelUserCloudPolicyController {
 public:
  FakeMachineLevelUserCloudPolicyController() = default;
  void FireNotification(bool succeeded) {
    NotifyPolicyRegisterFinished(succeeded);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeMachineLevelUserCloudPolicyController);
};

// A mock EnterpriseStartDialog to mimic the behavior of real dialog.
class MockEnterpriseStartupDialog : public EnterpriseStartupDialog {
 public:
  MockEnterpriseStartupDialog() = default;
  ~MockEnterpriseStartupDialog() override {
    // |callback_| exists if we're mocking the process that dialog is dismissed
    // automatically.
    if (callback_) {
      std::move(callback_).Run(false /* was_accepted */,
                               true /* can_show_browser_window */);
    }
  }

  MOCK_METHOD1(DisplayLaunchingInformationWithThrobber,
               void(const base::string16&));
  MOCK_METHOD2(DisplayErrorMessage,
               void(const base::string16&,
                    const base::Optional<base::string16>&));
  MOCK_METHOD0(IsShowing, bool());

  void SetCallback(EnterpriseStartupDialog::DialogResultCallback callback) {
    callback_ = std::move(callback);
  }

  void UserClickedTheButton(bool confirmed) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), confirmed,
                                  false /* can_show_browser_window */));
  }

 private:
  DialogResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MockEnterpriseStartupDialog);
};

}  // namespace

class MachineLevelUserCloudPolicyRegisterWatcherTest : public ::testing::Test {
 public:
  MachineLevelUserCloudPolicyRegisterWatcherTest()
      : watcher_(&controller_),
        dialog_(std::make_unique<MockEnterpriseStartupDialog>()),
        dialog_ptr_(dialog_.get()) {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.set_enrollment_token(kEnrollmentToken);
    storage_.set_dm_token(std::string());
    watcher_.SetDialogCreationCallbackForTesting(
        base::BindOnce(&MachineLevelUserCloudPolicyRegisterWatcherTest::
                           CreateEnterpriseStartupDialog,
                       base::Unretained(this)));
  }

 protected:
  FakeDMTokenStorage* storage() { return &storage_; }
  FakeMachineLevelUserCloudPolicyController* controller() {
    return &controller_;
  }
  MachineLevelUserCloudPolicyRegisterWatcher* watcher() { return &watcher_; }
  MockEnterpriseStartupDialog* dialog() { return dialog_ptr_; }

  std::unique_ptr<EnterpriseStartupDialog> CreateEnterpriseStartupDialog(
      EnterpriseStartupDialog::DialogResultCallback callback) {
    dialog_->SetCallback(std::move(callback));
    return std::move(dialog_);
  }

 private:
  content::TestBrowserThreadBundle browser_thread_bundle_;

  FakeMachineLevelUserCloudPolicyController controller_;
  MachineLevelUserCloudPolicyRegisterWatcher watcher_;
  FakeDMTokenStorage storage_;
  std::unique_ptr<MockEnterpriseStartupDialog> dialog_;
  MockEnterpriseStartupDialog* dialog_ptr_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyRegisterWatcherTest);
};

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest,
       NoEnrollmentNeededWithDMToken) {
  storage()->set_dm_token(kDMToken);
  EXPECT_EQ(RegisterResult::kEnrollmentSuccess,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
}

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest,
       NoEnrollmentNeededWithoutEnrollmentToken) {
  storage()->set_enrollment_token(std::string());
  storage()->set_dm_token(std::string());
  EXPECT_EQ(RegisterResult::kNoEnrollmentNeeded,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
}

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest, EnrollmentSucceed) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeMachineLevelUserCloudPolicyController::FireNotification,
          base::Unretained(controller()), true));
  EXPECT_EQ(RegisterResult::kEnrollmentSuccess,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kClosedSuccess,
      1);
}

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest,
       EnrollmentFailedAndQuit) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), DisplayErrorMessage(_, _))
      .WillOnce(
          InvokeWithoutArgs([this] { dialog()->UserClickedTheButton(false); }));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeMachineLevelUserCloudPolicyController::FireNotification,
          base::Unretained(controller()), false));
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kClosedFail,
      1);
}

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest,
       EnrollmentFailedAndRestart) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  EXPECT_CALL(*dialog(), DisplayErrorMessage(_, _))
      .WillOnce(
          InvokeWithoutArgs([this] { dialog()->UserClickedTheButton(true); }));
  EXPECT_CALL(*dialog(), IsShowing()).WillOnce(Return(true));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeMachineLevelUserCloudPolicyController::FireNotification,
          base::Unretained(controller()), false));
  EXPECT_EQ(RegisterResult::kRestartDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kClosedRelaunch,
      1);
}

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest,
       EnrollmentCanceledBeforeFinish) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayLaunchingInformationWithThrobber(_));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockEnterpriseStartupDialog::UserClickedTheButton,
                     base::Unretained(dialog()), false));
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kClosedAbort,
      1);
}

TEST_F(MachineLevelUserCloudPolicyRegisterWatcherTest,
       EnrollmentFailedBeforeDialogDisplay) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(*dialog(), DisplayErrorMessage(_, _))
      .WillOnce(
          InvokeWithoutArgs([this] { dialog()->UserClickedTheButton(false); }));
  controller()->FireNotification(false);
  EXPECT_EQ(RegisterResult::kQuitDueToFailure,
            watcher()->WaitUntilCloudPolicyEnrollmentFinished());
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kShown,
      1);
  histogram_tester.ExpectBucketCount(
      MachineLevelUserCloudPolicyRegisterWatcher::kStartupDialogHistogramName,
      MachineLevelUserCloudPolicyRegisterWatcher::EnrollmentStartupDialog::
          kClosedFail,
      1);
}

}  // namespace policy
