// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"

#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_cache.h"

namespace chromeos {

namespace device_sync {

FakeDeviceSyncClient::FakeDeviceSyncClient() = default;

FakeDeviceSyncClient::~FakeDeviceSyncClient() = default;

void FakeDeviceSyncClient::ForceEnrollmentNow(
    mojom::DeviceSync::ForceEnrollmentNowCallback callback) {
  std::move(callback).Run(force_enrollment_now_success_);
}

void FakeDeviceSyncClient::ForceSyncNow(
    mojom::DeviceSync::ForceSyncNowCallback callback) {
  std::move(callback).Run(force_sync_now_success_);
}

cryptauth::RemoteDeviceRefList FakeDeviceSyncClient::GetSyncedDevices() {
  return synced_devices_;
}

base::Optional<cryptauth::RemoteDeviceRef>
FakeDeviceSyncClient::GetLocalDeviceMetadata() {
  return local_device_metadata_;
}

void FakeDeviceSyncClient::SetSoftwareFeatureState(
    const std::string public_key,
    cryptauth::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) {
  set_software_feature_state_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::FindEligibleDevices(
    cryptauth::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  find_eligible_devices_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::GetDebugInfo(
    mojom::DeviceSync::GetDebugInfoCallback callback) {
  get_debug_info_callback_queue_.push(std::move(callback));
}

void FakeDeviceSyncClient::InvokePendingSetSoftwareFeatureStateCallback(
    const base::Optional<std::string>& error_code) {
  std::move(set_software_feature_state_callback_queue_.front()).Run(error_code);
  set_software_feature_state_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingFindEligibleDevicesCallback(
    const base::Optional<std::string>& error_code,
    cryptauth::RemoteDeviceRefList eligible_devices,
    cryptauth::RemoteDeviceRefList ineligible_devices) {
  std::move(find_eligible_devices_callback_queue_.front())
      .Run(error_code, eligible_devices, ineligible_devices);
  find_eligible_devices_callback_queue_.pop();
}

void FakeDeviceSyncClient::InvokePendingGetDebugInfoCallback(
    mojom::DebugInfoPtr debug_info_ptr) {
  std::move(get_debug_info_callback_queue_.front())
      .Run(std::move(debug_info_ptr));
  get_debug_info_callback_queue_.pop();
}

}  // namespace device_sync

}  // namespace chromeos