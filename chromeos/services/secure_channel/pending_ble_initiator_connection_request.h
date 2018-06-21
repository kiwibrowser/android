// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_INITIATOR_CONNECTION_REQUEST_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_INITIATOR_CONNECTION_REQUEST_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/pending_connection_request_base.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

// ConnectionRequest corresponding to BLE connections in the initiator role.
class PendingBleInitiatorConnectionRequest
    : public PendingConnectionRequestBase<BleInitiatorFailureType> {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionRequest<BleInitiatorFailureType>>
    BuildInstance(std::unique_ptr<ClientConnectionParameters>
                      client_connection_parameters,
                  ConnectionPriority connection_priority,
                  PendingConnectionRequestDelegate* delegate);

   private:
    static Factory* test_factory_;
  };

  ~PendingBleInitiatorConnectionRequest() override;

 private:
  static const size_t kMaxEmptyScansPerDevice;
  static const size_t kMaxGattConnectionAttemptsPerDevice;

  PendingBleInitiatorConnectionRequest(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate);

  // PendingConnectionRequest<BleInitiatorFailureType>:
  void HandleConnectionFailure(BleInitiatorFailureType failure_detail) override;

  size_t num_empty_scan_failures_ = 0u;
  size_t num_gatt_failures_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PendingBleInitiatorConnectionRequest);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_INITIATOR_CONNECTION_REQUEST_H_
