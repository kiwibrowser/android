// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/components/tether/tether_host_fetcher.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/cryptauth/remote_device_provider.h"
#include "components/cryptauth/remote_device_ref.h"

namespace cryptauth {
class RemoteDeviceProvider;
}  // namespace cryptauth

namespace chromeos {

namespace tether {

// Concrete TetherHostFetcher implementation. Despite the asynchronous function
// prototypes, callbacks are invoked synchronously.
//
// Note: TetherHostFetcherImpl, and the Tether feature as a whole, is currently
// in the middle of a migration from using RemoteDeviceProvider to
// DeviceSyncClient. Its constructor accepts both objects, but expects only of
// them to be valid, and the other null (this is controlled at a higher level by
// features::kMultiDeviceApi). Once Tether has fully migrated to DeviceSync Mojo
// Service, RemoteDeviceProvider will be ripped out of this class. See
// https://crbug.com/848956.
class TetherHostFetcherImpl : public TetherHostFetcher,
                              public cryptauth::RemoteDeviceProvider::Observer,
                              public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<TetherHostFetcher> NewInstance(
        cryptauth::RemoteDeviceProvider* remote_device_provider,
        device_sync::DeviceSyncClient* device_sync_client);

    static void SetInstanceForTesting(Factory* factory);

   protected:
    virtual std::unique_ptr<TetherHostFetcher> BuildInstance(
        cryptauth::RemoteDeviceProvider* remote_device_provider,
        device_sync::DeviceSyncClient* device_sync_client);

   private:
    static Factory* factory_instance_;
  };

  ~TetherHostFetcherImpl() override;

  // TetherHostFetcher:
  bool HasSyncedTetherHosts() override;
  void FetchAllTetherHosts(const TetherHostListCallback& callback) override;
  void FetchTetherHost(const std::string& device_id,
                       const TetherHostCallback& callback) override;

  // cryptauth::RemoteDeviceProvider::Observer:
  void OnSyncDeviceListChanged() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

 protected:
  // TODO(crbug.com/848956): Remove RemoteDeviceProvider once all clients have
  // migrated to the DeviceSync Mojo API.
  TetherHostFetcherImpl(cryptauth::RemoteDeviceProvider* remote_device_provider,
                        device_sync::DeviceSyncClient* device_sync_client);

 private:
  void CacheCurrentTetherHosts();

  cryptauth::RemoteDeviceProvider* remote_device_provider_;
  device_sync::DeviceSyncClient* device_sync_client_;

  cryptauth::RemoteDeviceRefList current_remote_device_list_;

  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcherImpl);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_IMPL_H_
