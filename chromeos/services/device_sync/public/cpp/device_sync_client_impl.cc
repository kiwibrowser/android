// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"

#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/device_sync/public/mojom/constants.mojom.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/cryptauth/expiring_remote_device_cache.h"
#include "components/cryptauth/remote_device.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace device_sync {

// static
DeviceSyncClientImpl::Factory* DeviceSyncClientImpl::Factory::test_factory_ =
    nullptr;

// static
DeviceSyncClientImpl::Factory* DeviceSyncClientImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void DeviceSyncClientImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

DeviceSyncClientImpl::Factory::~Factory() = default;

std::unique_ptr<DeviceSyncClient> DeviceSyncClientImpl::Factory::BuildInstance(
    service_manager::Connector* connector) {
  return base::WrapUnique(new DeviceSyncClientImpl(connector));
}

DeviceSyncClientImpl::DeviceSyncClientImpl(
    service_manager::Connector* connector)
    : DeviceSyncClientImpl(connector, base::ThreadTaskRunnerHandle::Get()) {}

DeviceSyncClientImpl::DeviceSyncClientImpl(
    service_manager::Connector* connector,
    scoped_refptr<base::TaskRunner> task_runner)
    : binding_(this),
      expiring_device_cache_(
          std::make_unique<cryptauth::ExpiringRemoteDeviceCache>()),
      weak_ptr_factory_(this) {
  connector->BindInterface(mojom::kServiceName, &device_sync_ptr_);
  device_sync_ptr_->AddObserver(GenerateInterfacePtr(), base::OnceClosure());

  // Delay calling these until after the constructor finishes.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&DeviceSyncClientImpl::LoadLocalDeviceMetadata,
                                weak_ptr_factory_.GetWeakPtr()));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DeviceSyncClientImpl::LoadSyncedDevices,
                                       weak_ptr_factory_.GetWeakPtr()));
}

DeviceSyncClientImpl::~DeviceSyncClientImpl() = default;

void DeviceSyncClientImpl::OnEnrollmentFinished() {
  // Before notifying observers that enrollment has finished, sync down the
  // local device metadata. This ensures that observers will have access to the
  // metadata of the newly-synced local device as soon as
  // NotifyOnEnrollmentFinished() is invoked.
  LoadLocalDeviceMetadata();
}

void DeviceSyncClientImpl::OnNewDevicesSynced() {
  // Before notifying observers that new devices have synced, sync down the new
  // devices. This ensures that observers will have access to the synced devices
  // as soon as NotifyOnNewDevicesSynced() is invoked.
  LoadSyncedDevices();
}

void DeviceSyncClientImpl::ForceEnrollmentNow(
    mojom::DeviceSync::ForceEnrollmentNowCallback callback) {
  device_sync_ptr_->ForceEnrollmentNow(std::move(callback));
}

void DeviceSyncClientImpl::ForceSyncNow(
    mojom::DeviceSync::ForceSyncNowCallback callback) {
  device_sync_ptr_->ForceSyncNow(std::move(callback));
}

cryptauth::RemoteDeviceRefList DeviceSyncClientImpl::GetSyncedDevices() {
  return expiring_device_cache_->GetNonExpiredRemoteDevices();
}

base::Optional<cryptauth::RemoteDeviceRef>
DeviceSyncClientImpl::GetLocalDeviceMetadata() {
  return local_device_id_
             ? expiring_device_cache_->GetRemoteDevice(*local_device_id_)
             : base::nullopt;
}

void DeviceSyncClientImpl::SetSoftwareFeatureState(
    const std::string public_key,
    cryptauth::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    mojom::DeviceSync::SetSoftwareFeatureStateCallback callback) {
  device_sync_ptr_->SetSoftwareFeatureState(
      public_key, software_feature, enabled, is_exclusive, std::move(callback));
}

void DeviceSyncClientImpl::FindEligibleDevices(
    cryptauth::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  device_sync_ptr_->FindEligibleDevices(
      software_feature,
      base::BindOnce(&DeviceSyncClientImpl::OnFindEligibleDevicesCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceSyncClientImpl::GetDebugInfo(
    mojom::DeviceSync::GetDebugInfoCallback callback) {
  device_sync_ptr_->GetDebugInfo(std::move(callback));
}

void DeviceSyncClientImpl::LoadSyncedDevices() {
  device_sync_ptr_->GetSyncedDevices(
      base::BindOnce(&DeviceSyncClientImpl::OnGetSyncedDevicesCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSyncClientImpl::LoadLocalDeviceMetadata() {
  device_sync_ptr_->GetLocalDeviceMetadata(
      base::BindOnce(&DeviceSyncClientImpl::OnGetLocalDeviceMetadataCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceSyncClientImpl::OnGetSyncedDevicesCompleted(
    const base::Optional<std::vector<cryptauth::RemoteDevice>>&
        remote_devices) {
  if (!remote_devices) {
    PA_LOG(INFO) << "Tried to fetch synced devices before service was fully "
                    "initialized; waiting for sync to complete before "
                    "continuing.";
    return;
  }

  if (waiting_for_local_device_metadata_) {
    waiting_for_local_device_metadata_ = false;
    LoadLocalDeviceMetadata();
  }

  expiring_device_cache_->SetRemoteDevicesAndInvalidateOldEntries(
      *remote_devices);

  NotifyNewDevicesSynced();
}

void DeviceSyncClientImpl::OnGetLocalDeviceMetadataCompleted(
    const base::Optional<cryptauth::RemoteDevice>& local_device_metadata) {
  if (!local_device_metadata) {
    PA_LOG(INFO) << "Tried to get local device metadata before service was "
                    "fully initialized; waiting for enrollment to complete "
                    "before continuing.";
    waiting_for_local_device_metadata_ = true;
    return;
  }

  local_device_id_ = local_device_metadata->GetDeviceId();
  expiring_device_cache_->UpdateRemoteDevice(*local_device_metadata);

  NotifyEnrollmentFinished();
}

void DeviceSyncClientImpl::OnFindEligibleDevicesCompleted(
    FindEligibleDevicesCallback callback,
    const base::Optional<std::string>& error_code,
    mojom::FindEligibleDevicesResponsePtr response) {
  cryptauth::RemoteDeviceRefList eligible_devices;
  cryptauth::RemoteDeviceRefList ineligible_devices;

  if (!error_code) {
    std::transform(
        response->eligible_devices.begin(), response->eligible_devices.end(),
        std::back_inserter(eligible_devices), [this](const auto& device) {
          return *expiring_device_cache_->GetRemoteDevice(device.GetDeviceId());
        });
    std::transform(
        response->ineligible_devices.begin(),
        response->ineligible_devices.end(),
        std::back_inserter(ineligible_devices), [this](const auto& device) {
          return *expiring_device_cache_->GetRemoteDevice(device.GetDeviceId());
        });
  }

  std::move(callback).Run(error_code, eligible_devices, ineligible_devices);
}

mojom::DeviceSyncObserverPtr DeviceSyncClientImpl::GenerateInterfacePtr() {
  mojom::DeviceSyncObserverPtr interface_ptr;
  binding_.Bind(mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void DeviceSyncClientImpl::FlushForTesting() {
  device_sync_ptr_.FlushForTesting();
}

}  // namespace device_sync

}  // namespace chromeos
