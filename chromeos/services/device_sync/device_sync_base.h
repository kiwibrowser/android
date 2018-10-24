// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_

#include "base/macros.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/signin/core/browser/account_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace chromeos {

namespace device_sync {

// Base DeviceSync implementation.
class DeviceSyncBase : public mojom::DeviceSync {
 public:
  ~DeviceSyncBase() override;

  // mojom::DeviceSync:
  void AddObserver(mojom::DeviceSyncObserverPtr observer,
                   AddObserverCallback callback) override;

  // Binds a request to this implementation. Should be called each time that the
  // service receives a request.
  void BindRequest(mojom::DeviceSyncRequest request);

 protected:
  DeviceSyncBase();

  void NotifyOnEnrollmentFinished();
  void NotifyOnNewDevicesSynced();

 private:
  mojo::InterfacePtrSet<mojom::DeviceSyncObserver> observers_;
  mojo::BindingSet<mojom::DeviceSync> bindings_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncBase);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_