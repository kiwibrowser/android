// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromeos {

namespace {

class MockDemoSetupControllerDelegate : public DemoSetupController::Delegate {
 public:
  MockDemoSetupControllerDelegate()
      : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~MockDemoSetupControllerDelegate() override = default;

  void OnSetupError(bool fatal) override {
    EXPECT_FALSE(succeeded_.has_value());
    succeeded_ = false;
    fatal_ = fatal;
    run_loop_->Quit();
  }

  void OnSetupSuccess() override {
    EXPECT_FALSE(succeeded_.has_value());
    succeeded_ = true;
    run_loop_->Quit();
  }

  // Wait until the setup result arrives (either OnSetupError or OnSetupSuccess
  // is called), returns true when the result matches with |expected|.
  bool WaitResult(bool expected) {
    // Run() stops immediately if Quit is already called.
    run_loop_->Run();
    return succeeded_.has_value() && succeeded_.value() == expected;
  }

  // Returns true if it receives a fatal error.
  bool IsErrorFatal() const { return fatal_; }

  void Reset() {
    succeeded_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  base::Optional<bool> succeeded_;
  bool fatal_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MockDemoSetupControllerDelegate);
};

bool SetupDummyOfflineDir(const std::string& account_id,
                          base::ScopedTempDir* temp_dir) {
  if (!temp_dir->CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create unique tempdir";
    return false;
  }

  if (base::WriteFile(temp_dir->GetPath().AppendASCII("device_policy"), "",
                      0) != 0) {
    LOG(ERROR) << "Failed to create device_policy file";
    return false;
  }

  // We use MockCloudPolicyStore for the device local account policy in the
  // tests, thus actual policy content can be empty. account_id is specified
  // since it is used by DemoSetupController to look up the store.
  std::string policy_blob;
  if (!account_id.empty()) {
    enterprise_management::PolicyData policy_data;
    policy_data.set_username(account_id);
    enterprise_management::PolicyFetchResponse policy;
    policy.set_policy_data(policy_data.SerializeAsString());
    policy_blob = policy.SerializeAsString();
  }
  if (base::WriteFile(temp_dir->GetPath().AppendASCII("local_account_policy"),
                      policy_blob.data(), policy_blob.size()) !=
      static_cast<int>(policy_blob.size())) {
    LOG(ERROR) << "Failed to create local_account_policy file";
    return false;
  }
  return true;
}

}  // namespace

class DemoSetupControllerTest : public testing::Test {
 protected:
  enum class SetupResult { SUCCESS, ERROR };

  template <SetupResult result>
  static EnterpriseEnrollmentHelper* MockOnlineEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);

    EXPECT_EQ(enrollment_config.mode,
              policy::EnrollmentConfig::MODE_ATTESTATION);
    EXPECT_CALL(*mock, EnrollUsingAttestation())
        .WillRepeatedly(testing::Invoke([mock]() {
          if (result == SetupResult::SUCCESS) {
            mock->status_consumer()->OnDeviceEnrolled("");
          } else {
            // TODO(agawronska): Test different error types.
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForStatus(
                    policy::EnrollmentStatus::REGISTRATION_FAILED));
          }
        }));
    return mock;
  }

  template <SetupResult result>
  static EnterpriseEnrollmentHelper* MockOfflineEnrollmentHelperCreator(
      EnterpriseEnrollmentHelper::EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain) {
    EnterpriseEnrollmentHelperMock* mock =
        new EnterpriseEnrollmentHelperMock(status_consumer);

    EXPECT_EQ(enrollment_config.mode,
              policy::EnrollmentConfig::MODE_OFFLINE_DEMO);
    EXPECT_CALL(*mock, EnrollForOfflineDemo())
        .WillRepeatedly(testing::Invoke([mock]() {
          if (result == SetupResult::SUCCESS) {
            mock->status_consumer()->OnDeviceEnrolled("");
          } else {
            // TODO(agawronska): Test different error types.
            mock->status_consumer()->OnEnrollmentError(
                policy::EnrollmentStatus::ForStatus(
                    policy::EnrollmentStatus::Status::LOCK_ERROR));
          }
        }));
    return mock;
  }

  DemoSetupControllerTest() = default;
  ~DemoSetupControllerTest() override = default;

  void SetUp() override {
    SystemSaltGetter::Initialize();
    DBusThreadManager::Initialize();
    DeviceSettingsService::Initialize();
    delegate_ = std::make_unique<MockDemoSetupControllerDelegate>();
    tested_controller_ = std::make_unique<DemoSetupController>(delegate_.get());
  }

  void TearDown() override {
    DBusThreadManager::Shutdown();
    SystemSaltGetter::Shutdown();
    DeviceSettingsService::Shutdown();
  }

  std::unique_ptr<MockDemoSetupControllerDelegate> delegate_;
  std::unique_ptr<DemoSetupController> tested_controller_;

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupControllerTest);
};

TEST_F(DemoSetupControllerTest, OfflineSuccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflineDir("test", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::SUCCESS>);
  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_))
      .WillOnce(testing::InvokeWithoutArgs(
          &mock_store, &policy::MockCloudPolicyStore::NotifyStoreLoaded));
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->EnrollOffline(temp_dir.GetPath());
  EXPECT_TRUE(delegate_->WaitResult(true));
}

TEST_F(DemoSetupControllerTest, OfflineDeviceLocalAccountPolicyLoadFailure) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::SUCCESS>);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_)).Times(0);
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->EnrollOffline(
      base::FilePath(FILE_PATH_LITERAL("/no/such/path")));
  EXPECT_TRUE(delegate_->WaitResult(false));
  EXPECT_FALSE(delegate_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OfflineDeviceLocalAccountPolicyStoreFailed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflineDir("test", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::SUCCESS>);
  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_))
      .WillOnce(testing::InvokeWithoutArgs(
          &mock_store, &policy::MockCloudPolicyStore::NotifyStoreError));
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->EnrollOffline(temp_dir.GetPath());
  EXPECT_TRUE(delegate_->WaitResult(false));
  EXPECT_TRUE(delegate_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OfflineInvalidDeviceLocalAccountPolicyBlob) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflineDir("", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::SUCCESS>);

  tested_controller_->EnrollOffline(temp_dir.GetPath());
  EXPECT_TRUE(delegate_->WaitResult(false));
  EXPECT_TRUE(delegate_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OfflineError) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflineDir("test", &temp_dir));

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOfflineEnrollmentHelperCreator<SetupResult::ERROR>);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_)).Times(0);
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->EnrollOffline(temp_dir.GetPath());
  EXPECT_TRUE(delegate_->WaitResult(false));
  EXPECT_FALSE(delegate_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, OnlineSuccess) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOnlineEnrollmentHelperCreator<SetupResult::SUCCESS>);

  tested_controller_->EnrollOnline();
  EXPECT_TRUE(delegate_->WaitResult(true));
}

TEST_F(DemoSetupControllerTest, OnlineError) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOnlineEnrollmentHelperCreator<SetupResult::ERROR>);

  tested_controller_->EnrollOnline();
  EXPECT_TRUE(delegate_->WaitResult(false));
  EXPECT_FALSE(delegate_->IsErrorFatal());
}

TEST_F(DemoSetupControllerTest, EnrollTwice) {
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOnlineEnrollmentHelperCreator<SetupResult::ERROR>);

  tested_controller_->EnrollOnline();
  EXPECT_TRUE(delegate_->WaitResult(false));
  EXPECT_FALSE(delegate_->IsErrorFatal());

  delegate_->Reset();

  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockOnlineEnrollmentHelperCreator<SetupResult::SUCCESS>);

  tested_controller_->EnrollOnline();
  EXPECT_TRUE(delegate_->WaitResult(true));
}

}  //  namespace chromeos
