// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_DEVICE_SYNC_CLIENT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_DEVICE_SYNC_CLIENT_H_

#include <memory>
#include <queue>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/remote_device_ref.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace chromeos {

namespace device_sync {

// Test double implementation of DeviceSyncClient.
class FakeDeviceSyncClient : public DeviceSyncClient {
 public:
  FakeDeviceSyncClient();
  ~FakeDeviceSyncClient() override;

  void InvokePendingSetSoftwareFeatureStateCallback(
      const base::Optional<std::string>& error_code);
  void InvokePendingFindEligibleDevicesCallback(
      const base::Optional<std::string>& error_code,
      cryptauth::RemoteDeviceRefList eligible_devices,
      cryptauth::RemoteDeviceRefList ineligible_devices);
  void InvokePendingGetDebugInfoCallback(mojom::DebugInfoPtr debug_info_ptr);

  void set_force_enrollment_now_success(bool force_enrollment_now_success) {
    force_enrollment_now_success_ = force_enrollment_now_success;
  }

  void set_force_sync_now_success(bool force_sync_now_success) {
    force_sync_now_success_ = force_sync_now_success;
  }

  void set_synced_devices(cryptauth::RemoteDeviceRefList synced_devices) {
    synced_devices_ = synced_devices;
  }

  void set_local_device_metadata(
      base::Optional<cryptauth::RemoteDeviceRef> local_device_metadata) {
    local_device_metadata_ = local_device_metadata;
  }

  using DeviceSyncClient::NotifyEnrollmentFinished;
  using DeviceSyncClient::NotifyNewDevicesSynced;

 private:
  // DeviceSyncClient:
  void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) override;
  void ForceSyncNow(mojom::DeviceSync::ForceSyncNowCallback callback) override;
  cryptauth::RemoteDeviceRefList GetSyncedDevices() override;
  base::Optional<cryptauth::RemoteDeviceRef> GetLocalDeviceMetadata() override;
  void SetSoftwareFeatureState(
      const std::string public_key,
      cryptauth::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) override;
  void FindEligibleDevices(cryptauth::SoftwareFeature software_feature,
                           FindEligibleDevicesCallback callback) override;
  void GetDebugInfo(mojom::DeviceSync::GetDebugInfoCallback callback) override;

  bool force_enrollment_now_success_;
  bool force_sync_now_success_;
  cryptauth::RemoteDeviceRefList synced_devices_;
  base::Optional<cryptauth::RemoteDeviceRef> local_device_metadata_;

  std::queue<mojom::DeviceSync::SetSoftwareFeatureStateCallback>
      set_software_feature_state_callback_queue_;
  std::queue<FindEligibleDevicesCallback> find_eligible_devices_callback_queue_;
  std::queue<mojom::DeviceSync::GetDebugInfoCallback>
      get_debug_info_callback_queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceSyncClient);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_FAKE_DEVICE_SYNC_CLIENT_H_
