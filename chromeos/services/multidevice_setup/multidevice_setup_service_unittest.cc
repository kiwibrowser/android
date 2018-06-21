// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/services/multidevice_setup/fake_account_status_change_delegate.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class MultiDeviceSetupServiceTest : public testing::Test {
 protected:
  MultiDeviceSetupServiceTest() = default;
  ~MultiDeviceSetupServiceTest() override = default;

  void SetUp() override {
    fake_account_status_change_delegate_ =
        std::make_unique<FakeAccountStatusChangeDelegate>();
    connector_factory_ =
        service_manager::TestConnectorFactory::CreateForUniqueService(
            std::make_unique<MultiDeviceSetupService>());
  }

  mojom::MultiDeviceSetup* GetMultiDeviceSetup() {
    if (!multidevice_setup_) {
      EXPECT_EQ(nullptr, connector_);

      // Create the Connector and bind it to |multidevice_setup_|.
      connector_ = connector_factory_->CreateConnector();
      connector_->BindInterface(mojom::kServiceName, &multidevice_setup_);

      // Set |fake_account_status_change_delegate_|.
      CallSetAccountStatusChangeDelegate();
    }

    return multidevice_setup_.get();
  }

  FakeAccountStatusChangeDelegate* fake_account_status_change_delegate() {
    return fake_account_status_change_delegate_.get();
  }

  void CallSetAccountStatusChangeDelegate() {
    base::RunLoop run_loop;
    multidevice_setup_->SetAccountStatusChangeDelegate(
        fake_account_status_change_delegate_->GenerateInterfacePtr(),
        base::BindOnce(
            &MultiDeviceSetupServiceTest::OnNotificationPresenterRegistered,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void CallTriggerEventForDebugging(mojom::EventTypeForDebugging type) {
    base::RunLoop run_loop;
    GetMultiDeviceSetup()->TriggerEventForDebugging(
        type,
        base::BindOnce(&MultiDeviceSetupServiceTest::OnNotificationTriggered,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 private:
  void OnNotificationPresenterRegistered(
      const base::RepeatingClosure& quit_closure) {
    quit_closure.Run();
  }

  void OnNotificationTriggered(const base::RepeatingClosure& quit_closure,
                               bool success) {
    // NotificationPresenter is set in GetMultiDeviceSetup().
    EXPECT_TRUE(success);
    quit_closure.Run();
  }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<service_manager::TestConnectorFactory> connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;

  std::unique_ptr<FakeAccountStatusChangeDelegate>
      fake_account_status_change_delegate_;
  mojom::MultiDeviceSetupPtr multidevice_setup_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupServiceTest);
};

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebugging_kNewUserPotentialHostExists) {
  CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kNewUserPotentialHostExists);

  EXPECT_EQ(
      1u, fake_account_status_change_delegate()->num_new_user_events_handled());
}

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebugging_kExistingUserConnectedHostSwitched) {
  CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserConnectedHostSwitched);

  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_existing_user_host_switched_events_handled());
}

TEST_F(MultiDeviceSetupServiceTest,
       TriggerEventForDebugging_kExistingUserNewChromebookAdded) {
  CallTriggerEventForDebugging(
      mojom::EventTypeForDebugging::kExistingUserNewChromebookAdded);

  EXPECT_EQ(1u, fake_account_status_change_delegate()
                    ->num_existing_user_chromebook_added_events_handled());
}

}  // namespace multidevice_setup

}  // namespace chromeos
