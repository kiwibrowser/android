// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/platform/system_provider_impl.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/battery_monitor.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace assistant {

class FakeBatteryMonitor : device::mojom::BatteryMonitor {
 public:
  FakeBatteryMonitor() : binding_(this) {}

  device::mojom::BatteryMonitorPtr CreateInterfacePtrAndBind() {
    device::mojom::BatteryMonitorPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  void QueryNextStatus(QueryNextStatusCallback callback) override {
    if (has_status) {
      std::move(callback).Run(std::move(battery_status_));
      has_status = false;
    } else {
      callback_ = std::move(callback);
    }
  }

  void SetStatus(device::mojom::BatteryStatusPtr battery_status) {
    battery_status_ = std::move(battery_status);
    has_status = true;

    if (callback_) {
      std::move(callback_).Run(std::move(battery_status_));
      has_status = false;
    }
  }

 private:
  bool has_status = false;

  device::mojom::BatteryStatusPtr battery_status_;
  QueryNextStatusCallback callback_;

  mojo::Binding<device::mojom::BatteryMonitor> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeBatteryMonitor);
};

class SystemProviderImplTest : public testing::Test {
 public:
  SystemProviderImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {
    battery_monitor_.SetStatus(device::mojom::BatteryStatus::New(
        false /* charging */, 0 /* charging_time */, 0 /* discharging_time */,
        0 /* level */));

    system_provider_impl_ = std::make_unique<SystemProviderImpl>(
        battery_monitor_.CreateInterfacePtrAndBind());
    FlushForTesting();
  }

  SystemProviderImpl* system_provider() { return system_provider_impl_.get(); }

  FakeBatteryMonitor* battery_monitor() { return &battery_monitor_; }

  void FlushForTesting() { system_provider_impl_->FlushForTesting(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  FakeBatteryMonitor battery_monitor_;
  std::unique_ptr<SystemProviderImpl> system_provider_impl_;

  DISALLOW_COPY_AND_ASSIGN(SystemProviderImplTest);
};

TEST_F(SystemProviderImplTest, GetBatteryStateReturnsLastState) {
  SystemProviderImpl::BatteryState state;
  // Initial level is 0
  system_provider()->GetBatteryState(&state);
  FlushForTesting();

  EXPECT_EQ(state.charge_percentage, 0);
  battery_monitor()->SetStatus(device::mojom::BatteryStatus::New(
      false /* charging */, 0 /* charging_time */, 0 /* discharging_time */,
      1 /* level */));

  FlushForTesting();
  // New level after status change
  system_provider()->GetBatteryState(&state);
  EXPECT_EQ(state.charge_percentage, 100);
}

}  // namespace assistant
}  // namespace chromeos
