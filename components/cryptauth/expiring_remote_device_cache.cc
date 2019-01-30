// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/expiring_remote_device_cache.h"

#include "base/stl_util.h"
#include "components/cryptauth/remote_device_cache.h"

namespace cryptauth {

ExpiringRemoteDeviceCache::ExpiringRemoteDeviceCache()
    : remote_device_cache_(RemoteDeviceCache::Factory::Get()->BuildInstance()) {
}

ExpiringRemoteDeviceCache::~ExpiringRemoteDeviceCache() = default;

void ExpiringRemoteDeviceCache::SetRemoteDevicesAndInvalidateOldEntries(
    const RemoteDeviceList& remote_devices) {
  remote_device_cache_->SetRemoteDevices(remote_devices);

  device_ids_from_last_set_call_.clear();
  for (const auto& device : remote_devices)
    device_ids_from_last_set_call_.insert(device.GetDeviceId());
}

RemoteDeviceRefList ExpiringRemoteDeviceCache::GetNonExpiredRemoteDevices()
    const {
  // Only add to the output list if the entry is not stale.
  RemoteDeviceRefList remote_devices;
  for (auto device : remote_device_cache_->GetRemoteDevices()) {
    if (device_ids_from_last_set_call_.count(device.GetDeviceId()) != 0)
      remote_devices.push_back(device);
  }

  return remote_devices;
}

void ExpiringRemoteDeviceCache::UpdateRemoteDevice(
    const RemoteDevice& remote_device) {
  remote_device_cache_->SetRemoteDevices({remote_device});
  device_ids_from_last_set_call_.insert(remote_device.GetDeviceId());
}

base::Optional<RemoteDeviceRef> ExpiringRemoteDeviceCache::GetRemoteDevice(
    const std::string& device_id) const {
  return remote_device_cache_->GetRemoteDevice(device_id);
}

}  // namespace cryptauth
