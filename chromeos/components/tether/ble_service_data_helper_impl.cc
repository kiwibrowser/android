// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_service_data_helper_impl.h"

#include <algorithm>
#include <iterator>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "components/cryptauth/background_eid_generator.h"
#include "components/cryptauth/ble/ble_advertisement_generator.h"
#include "components/cryptauth/foreground_eid_generator.h"
#include "components/cryptauth/local_device_data_provider.h"

namespace chromeos {

namespace tether {

namespace {

// Valid advertisement service data must be at least 2 bytes.
// As of March 2018, valid background advertisement service data is exactly 2
// bytes, which identify the advertising device to the scanning device.
// Valid foreground advertisement service data must include at least 4 bytes:
// 2 bytes associated with the scanning device (used as a scan filter) and 2
// bytes which identify the advertising device to the scanning device.
const size_t kMinNumBytesInServiceData = 2;
const size_t kMaxNumBytesInBackgroundServiceData = 3;
const size_t kMinNumBytesInForegroundServiceData = 4;

}  // namespace

// static
BleServiceDataHelperImpl::Factory*
    BleServiceDataHelperImpl::Factory::test_factory_ = nullptr;

// static
BleServiceDataHelperImpl::Factory* BleServiceDataHelperImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleServiceDataHelperImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleServiceDataHelperImpl::Factory::~Factory() = default;

std::unique_ptr<secure_channel::BleServiceDataHelper>
BleServiceDataHelperImpl::Factory::BuildInstance(
    TetherHostFetcher* tether_host_fetcher,
    cryptauth::LocalDeviceDataProvider* local_device_data_provider,
    device_sync::DeviceSyncClient* device_sync_client) {
  return base::WrapUnique(new BleServiceDataHelperImpl(
      tether_host_fetcher, local_device_data_provider, device_sync_client));
}

BleServiceDataHelperImpl::BleServiceDataHelperImpl(
    TetherHostFetcher* tether_host_fetcher,
    cryptauth::LocalDeviceDataProvider* local_device_data_provider,
    device_sync::DeviceSyncClient* device_sync_client)
    : tether_host_fetcher_(tether_host_fetcher),
      local_device_data_provider_(local_device_data_provider),
      device_sync_client_(device_sync_client),
      background_eid_generator_(
          std::make_unique<cryptauth::BackgroundEidGenerator>()),
      foreground_eid_generator_(
          std::make_unique<cryptauth::ForegroundEidGenerator>()),
      weak_ptr_factory_(this) {
  tether_host_fetcher_->AddObserver(this);
  OnTetherHostsUpdated();
}

BleServiceDataHelperImpl::~BleServiceDataHelperImpl() {
  tether_host_fetcher_->RemoveObserver(this);
}

base::Optional<secure_channel::BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelperImpl::IdentifyRemoteDevice(
    const std::string& service_data,
    const std::vector<std::string>& remote_device_ids) {
  base::Optional<std::string> local_device_public_key =
      GetLocalDevicePublicKey();
  if (!local_device_public_key)
    return base::nullopt;

  // BleServiceDataHelper::IdentifyRemoteDevice() verifies that the devices its
  // subclasses return are actually in |device_id_pair_set|. However, clients of
  // BleServiceDataHelperImpl don't have easy access to local device metadata;
  // therefore, pass along that required data here.
  secure_channel::DeviceIdPairSet device_id_pair_set;
  for (const auto& remote_device_id : remote_device_ids) {
    device_id_pair_set.insert(secure_channel::DeviceIdPair(
        remote_device_id, cryptauth::RemoteDeviceRef::GenerateDeviceId(
                              *local_device_public_key)));
  }

  return secure_channel::BleServiceDataHelper::IdentifyRemoteDevice(
      service_data, device_id_pair_set);
}

std::unique_ptr<cryptauth::DataWithTimestamp>
BleServiceDataHelperImpl::GenerateForegroundAdvertisement(
    const secure_channel::DeviceIdPair& device_id_pair) {
  base::Optional<std::string> local_device_public_key =
      GetLocalDevicePublicKey();
  if (!local_device_public_key) {
    PA_LOG(ERROR) << "Local device public key is invalid.";
    return nullptr;
  }

  base::Optional<cryptauth::RemoteDeviceRef> remote_device;
  const auto remote_device_it = std::find_if(
      tether_hosts_from_last_fetch_.begin(),
      tether_hosts_from_last_fetch_.end(),
      [&device_id_pair](auto remote_device) {
        return device_id_pair.remote_device_id() == remote_device.GetDeviceId();
      });

  if (remote_device_it == tether_hosts_from_last_fetch_.end()) {
    PA_LOG(WARNING) << "Requested remote device ID is not a valid host: "
                    << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                           device_id_pair.remote_device_id());
    return nullptr;
  }

  return cryptauth::BleAdvertisementGenerator::GenerateBleAdvertisement(
      *remote_device_it, *local_device_public_key);
}

base::Optional<secure_channel::BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const secure_channel::DeviceIdPairSet& device_id_pair_set) {
  std::vector<std::string> remote_device_ids;
  for (const auto& device_id_pair : device_id_pair_set) {
    // It's fine to ignore device_id_pair.local_device_id(); it's the same for
    // each entry.
    remote_device_ids.push_back(device_id_pair.remote_device_id());
  }

  std::string device_id;
  bool is_background_advertisement = false;

  // First try, identifying |service_data| as a foreground advertisement.
  if (service_data.size() >= kMinNumBytesInForegroundServiceData) {
    std::vector<cryptauth::BeaconSeed> beacon_seeds;
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      beacon_seeds =
          device_sync_client_->GetLocalDeviceMetadata()->beacon_seeds();
    } else if (!local_device_data_provider_->GetLocalDeviceData(
                   nullptr, &beacon_seeds)) {
      PA_LOG(ERROR) << "Cannot fetch local beacon seeds.";
      return base::nullopt;
    }

    device_id = foreground_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
        service_data, remote_device_ids, beacon_seeds);
  }

  // If the device has not yet been identified, try identifying |service_data|
  // as a background advertisement.
  if (chromeos::switches::IsInstantTetheringBackgroundAdvertisingSupported() &&
      device_id.empty() && service_data.size() >= kMinNumBytesInServiceData &&
      service_data.size() <= kMaxNumBytesInBackgroundServiceData) {
    cryptauth::RemoteDeviceRefList remote_devices;
    for (auto remote_device : tether_hosts_from_last_fetch_) {
      if (base::ContainsValue(remote_device_ids, remote_device.GetDeviceId()))
        remote_devices.push_back(remote_device);
    }

    device_id = background_eid_generator_->IdentifyRemoteDeviceByAdvertisement(
        service_data, remote_devices);
    is_background_advertisement = true;
  }

  // If the service data does not correspond to an advertisement from a device
  // on this account, ignore it.
  if (device_id.empty())
    return base::nullopt;

  for (const auto& remote_device_ref : tether_hosts_from_last_fetch_) {
    if (remote_device_ref.GetDeviceId() == device_id) {
      return secure_channel::BleServiceDataHelper::DeviceWithBackgroundBool(
          remote_device_ref, is_background_advertisement);
    }
  }

  PA_LOG(ERROR) << "Identified remote device ID is not a valid host: "
                << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                       device_id);
  NOTREACHED();
  return base::nullopt;
}

void BleServiceDataHelperImpl::OnTetherHostsUpdated() {
  tether_host_fetcher_->FetchAllTetherHosts(
      base::Bind(&BleServiceDataHelperImpl::OnTetherHostsFetched,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BleServiceDataHelperImpl::OnTetherHostsFetched(
    const cryptauth::RemoteDeviceRefList& tether_hosts) {
  tether_hosts_from_last_fetch_ = tether_hosts;
}

base::Optional<std::string>
BleServiceDataHelperImpl::GetLocalDevicePublicKey() {
  std::string local_device_public_key;
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    if (!device_sync_client_->GetLocalDeviceMetadata())
      return base::nullopt;
    local_device_public_key =
        device_sync_client_->GetLocalDeviceMetadata()->public_key();
  } else if (!local_device_data_provider_->GetLocalDeviceData(
                 &local_device_public_key, nullptr)) {
    return base::nullopt;
  }

  if (local_device_public_key.empty())
    return base::nullopt;

  return local_device_public_key;
}

void BleServiceDataHelperImpl::SetTestDoubles(
    std::unique_ptr<cryptauth::BackgroundEidGenerator> background_eid_generator,
    std::unique_ptr<cryptauth::ForegroundEidGenerator>
        foreground_eid_generator) {
  background_eid_generator_ = std::move(background_eid_generator);
  foreground_eid_generator_ = std::move(foreground_eid_generator);
}

}  // namespace tether

}  // namespace chromeos
