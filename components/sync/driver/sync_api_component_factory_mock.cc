// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_api_component_factory_mock.h"

#include <utility>

#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

SyncApiComponentFactoryMock::SyncApiComponentFactoryMock() {
  // To avoid returning nulls and crashes in many tests, we default to create
  // a mock device info provider.
  ON_CALL(*this, CreateLocalDeviceInfoProvider())
      .WillByDefault(testing::Invoke(
          []() { return std::make_unique<LocalDeviceInfoProviderMock>(); }));
}

SyncApiComponentFactoryMock::~SyncApiComponentFactoryMock() = default;

}  // namespace syncer
