// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_EXPIRING_REMOTE_DEVICE_CACHE_H_
#define COMPONENTS_CRYPTAUTH_EXPIRING_REMOTE_DEVICE_CACHE_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "components/cryptauth/remote_device.h"
#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {

class RemoteDeviceCache;

// A helper class around RemoteDeviceCache which keeps track of which devices
// have been removed from, or "expired" in, the cache.
//
// If the set of devices provided to SetRemoteDevicesAndInvalidateOldEntries()
// is different from the set provided to a previous call to
// SetRemoteDevicesAndInvalidateOldEntries(), then the devices in the previous
// call which are not in the new call will be marked as stale. Stale devices are
// still valid RemoteDeviceRefs (preventing clients from segfaulting), but will
// not be returned by GetNonExpiredRemoteDevices().
class ExpiringRemoteDeviceCache {
 public:
  ExpiringRemoteDeviceCache();
  virtual ~ExpiringRemoteDeviceCache();

  void SetRemoteDevicesAndInvalidateOldEntries(
      const RemoteDeviceList& remote_devices);

  RemoteDeviceRefList GetNonExpiredRemoteDevices() const;

  // Add or update a RemoteDevice without marking any other devices in the cache
  // as stale.
  void UpdateRemoteDevice(const RemoteDevice& remote_device);

  base::Optional<RemoteDeviceRef> GetRemoteDevice(
      const std::string& device_id) const;

 private:
  std::unique_ptr<RemoteDeviceCache> remote_device_cache_;
  std::set<std::string> device_ids_from_last_set_call_;

  DISALLOW_COPY_AND_ASSIGN(ExpiringRemoteDeviceCache);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_EXPIRING_REMOTE_DEVICE_CACHE_H_
