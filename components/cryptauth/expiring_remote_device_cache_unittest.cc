// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/expiring_remote_device_cache.h"

#include <algorithm>

#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cryptauth {

class ExpiringRemoteDeviceCacheTest : public testing::Test {
 protected:
  ExpiringRemoteDeviceCacheTest()
      : test_remote_device_list_(CreateRemoteDeviceListForTest(5)),
        test_remote_device_ref_list_(CreateRemoteDeviceRefListForTest(5)){};

  // testing::Test:
  void SetUp() override {
    cache_ = std::make_unique<ExpiringRemoteDeviceCache>();
  }

  void VerifyCacheRemoteDevices(
      RemoteDeviceRefList expected_remote_device_ref_list) {
    RemoteDeviceRefList remote_device_ref_list =
        cache_->GetNonExpiredRemoteDevices();
    std::sort(remote_device_ref_list.begin(), remote_device_ref_list.end(),
              [](const auto& device_1, const auto& device_2) {
                return device_1 < device_2;
              });

    EXPECT_EQ(expected_remote_device_ref_list, remote_device_ref_list);
  }

  const RemoteDeviceList test_remote_device_list_;
  const RemoteDeviceRefList test_remote_device_ref_list_;
  std::unique_ptr<ExpiringRemoteDeviceCache> cache_;

  DISALLOW_COPY_AND_ASSIGN(ExpiringRemoteDeviceCacheTest);
};

TEST_F(ExpiringRemoteDeviceCacheTest,
       TestSetRemoteDevices_RemoteDeviceRefsRemoved) {
  cache_->SetRemoteDevicesAndInvalidateOldEntries(test_remote_device_list_);

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);

  cache_->SetRemoteDevicesAndInvalidateOldEntries(RemoteDeviceList());

  VerifyCacheRemoteDevices(RemoteDeviceRefList());
}

TEST_F(ExpiringRemoteDeviceCacheTest,
       TestSetRemoteDevices_DeviceRemovedAndAddedBack) {
  cache_->SetRemoteDevicesAndInvalidateOldEntries(test_remote_device_list_);
  cache_->SetRemoteDevicesAndInvalidateOldEntries(RemoteDeviceList());
  cache_->SetRemoteDevicesAndInvalidateOldEntries(test_remote_device_list_);

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);
}

TEST_F(ExpiringRemoteDeviceCacheTest, TestUpdateRemoteDevice) {
  cache_->SetRemoteDevicesAndInvalidateOldEntries(test_remote_device_list_);

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);

  cache_->UpdateRemoteDevice(test_remote_device_list_[0]);

  VerifyCacheRemoteDevices(test_remote_device_ref_list_);
}

}  // namespace cryptauth