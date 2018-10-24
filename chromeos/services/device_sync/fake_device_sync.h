// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_

#include <queue>

#include "base/macros.h"
#include "chromeos/services/device_sync/device_sync_base.h"

namespace chromeos {

namespace device_sync {

// Test double DeviceSync implementation.
class FakeDeviceSync : public DeviceSyncBase {
 public:
  FakeDeviceSync();
  ~FakeDeviceSync() override;

  using DeviceSyncBase::NotifyOnEnrollmentFinished;
  using DeviceSyncBase::NotifyOnNewDevicesSynced;

  void set_force_enrollment_now_completed_success(
      bool force_enrollment_now_completed_success) {
    force_enrollment_now_completed_success_ =
        force_enrollment_now_completed_success;
  }

  void set_force_sync_now_completed_success(
      bool force_sync_now_completed_success) {
    force_sync_now_completed_success_ = force_sync_now_completed_success;
  }

  void InvokePendingGetLocalDeviceMetadataCallback(
      const base::Optional<cryptauth::RemoteDevice>& local_device_metadata);
  void InvokePendingGetSyncedDevicesCallback(
      const base::Optional<std::vector<cryptauth::RemoteDevice>>&
          remote_devices);
  void InvokePendingSetSoftwareFeatureStateCallback(
      const base::Optional<std::string>& error_code);
  void InvokePendingFindEligibleDevicesCallback(
      const base::Optional<std::string>& error_code,
      mojom::FindEligibleDevicesResponsePtr find_eligible_devices_response_ptr);
  void InvokePendingGetDebugInfoCallback(mojom::DebugInfoPtr debug_info_ptr);

 protected:
  // mojom::DeviceSync:
  void ForceEnrollmentNow(ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(ForceSyncNowCallback callback) override;
  void GetLocalDeviceMetadata(GetLocalDeviceMetadataCallback callback) override;
  void GetSyncedDevices(GetSyncedDevicesCallback callback) override;
  void SetSoftwareFeatureState(
      const std::string& device_public_key,
      cryptauth::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      SetSoftwareFeatureStateCallback callback) override;
  void FindEligibleDevices(cryptauth::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;

 private:
  bool force_enrollment_now_completed_success_ = true;
  bool force_sync_now_completed_success_ = true;

  std::queue<GetLocalDeviceMetadataCallback>
      get_local_device_metadata_callback_queue_;
  std::queue<GetSyncedDevicesCallback> get_synced_devices_callback_queue_;
  std::queue<SetSoftwareFeatureStateCallback>
      set_software_feature_state_callback_queue_;
  std::queue<FindEligibleDevicesCallback> find_eligible_devices_callback_queue_;
  std::queue<GetDebugInfoCallback> get_debug_info_callback_queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceSync);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_FAKE_DEVICE_SYNC_H_
