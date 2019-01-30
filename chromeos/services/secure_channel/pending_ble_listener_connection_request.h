// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_

#include "base/macros.h"
#include "chromeos/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/services/secure_channel/client_connection_parameters.h"
#include "chromeos/services/secure_channel/pending_connection_request_base.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace chromeos {

namespace secure_channel {

// ConnectionRequest corresponding to BLE connections in the listener role.
class PendingBleListenerConnectionRequest
    : public PendingConnectionRequestBase<BleListenerFailureType> {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<PendingConnectionRequest<BleListenerFailureType>>
    BuildInstance(std::unique_ptr<ClientConnectionParameters>
                      client_connection_parameters,
                  ConnectionPriority connection_priority,
                  PendingConnectionRequestDelegate* delegate);

   private:
    static Factory* test_factory_;
  };

  ~PendingBleListenerConnectionRequest() override;

 private:
  PendingBleListenerConnectionRequest(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      ConnectionPriority connection_priority,
      PendingConnectionRequestDelegate* delegate);

  // PendingConnectionRequest<BleListenerFailureType>:
  void HandleConnectionFailure(BleListenerFailureType failure_detail) override;

  DISALLOW_COPY_AND_ASSIGN(PendingBleListenerConnectionRequest);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PENDING_BLE_LISTENER_CONNECTION_REQUEST_H_
