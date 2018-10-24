// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/remote_device_ref.h"

namespace chromeos {

namespace device_sync {

// Provides clients access to the DeviceSync API.
class DeviceSyncClient {
 public:
  class Observer {
   public:
    virtual void OnEnrollmentFinished() {}
    virtual void OnNewDevicesSynced() {}

   protected:
    virtual ~Observer() = default;
  };

  using FindEligibleDevicesCallback =
      base::OnceCallback<void(const base::Optional<std::string>&,
                              cryptauth::RemoteDeviceRefList,
                              cryptauth::RemoteDeviceRefList)>;

  DeviceSyncClient();
  virtual ~DeviceSyncClient();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual void ForceEnrollmentNow(
      mojom::DeviceSync::ForceEnrollmentNowCallback callback) = 0;
  virtual void ForceSyncNow(
      mojom::DeviceSync::ForceSyncNowCallback callback) = 0;
  virtual cryptauth::RemoteDeviceRefList GetSyncedDevices() = 0;
  virtual base::Optional<cryptauth::RemoteDeviceRef>
  GetLocalDeviceMetadata() = 0;

  // Note: In the special case of passing |software_feature| =
  // SoftwareFeature::EASY_UNLOCK_HOST and |enabled| = false, |public_key| is
  // ignored.
  virtual void SetSoftwareFeatureState(
      const std::string public_key,
      cryptauth::SoftwareFeature software_feature,
      bool enabled,
      bool is_exclusive,
      mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) = 0;
  virtual void FindEligibleDevices(cryptauth::SoftwareFeature software_feature,
                                   FindEligibleDevicesCallback callback) = 0;
  virtual void GetDebugInfo(
      mojom::DeviceSync::GetDebugInfoCallback callback) = 0;

 protected:
  void NotifyEnrollmentFinished();
  void NotifyNewDevicesSynced();

 private:
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClient);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_CLIENT_H_
