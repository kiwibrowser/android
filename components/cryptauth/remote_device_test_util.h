// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_TEST_UTIL_H_
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_TEST_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {

// Attributes of the default test remote device.
extern const char kTestRemoteDeviceName[];
extern const char kTestRemoteDevicePublicKey[];

class RemoteDeviceRefBuilder {
 public:
  RemoteDeviceRefBuilder();
  ~RemoteDeviceRefBuilder();
  RemoteDeviceRefBuilder& SetUserId(const std::string& user_id);
  RemoteDeviceRefBuilder& SetName(const std::string& name);
  RemoteDeviceRefBuilder& SetPublicKey(const std::string& public_key);
  RemoteDeviceRefBuilder& SetSupportsMobileHotspot(
      bool supports_mobile_hotspot);
  RemoteDeviceRefBuilder& SetSoftwareFeatureState(
      const SoftwareFeature feature,
      const SoftwareFeatureState new_state);
  RemoteDeviceRefBuilder& SetLastUpdateTimeMillis(
      int64_t last_update_time_millis);
  RemoteDeviceRefBuilder& SetBeaconSeeds(
      const std::vector<BeaconSeed>& beacon_seeds);
  RemoteDeviceRef Build();

 private:
  std::shared_ptr<RemoteDevice> remote_device_;
};

RemoteDevice CreateRemoteDeviceForTest();

RemoteDeviceRef CreateRemoteDeviceRefForTest();

RemoteDeviceList CreateRemoteDeviceListForTest(size_t num_to_create);

RemoteDeviceRefList CreateRemoteDeviceRefListForTest(size_t num_to_create);

RemoteDevice* GetMutableRemoteDevice(const RemoteDeviceRef& remote_device_ref);

bool IsSameDevice(const cryptauth::RemoteDevice& remote_device,
                  cryptauth::RemoteDeviceRef remote_device_ref);

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_TEST_UTIL_H_
