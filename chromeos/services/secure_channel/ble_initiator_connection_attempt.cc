// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_initiator_connection_attempt.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/secure_channel/ble_initiator_operation.h"

namespace chromeos {

namespace secure_channel {

// static
BleInitiatorConnectionAttempt::Factory*
    BleInitiatorConnectionAttempt::Factory::test_factory_ = nullptr;

// static
BleInitiatorConnectionAttempt::Factory*
BleInitiatorConnectionAttempt::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleInitiatorConnectionAttempt::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleInitiatorConnectionAttempt::Factory::~Factory() = default;

std::unique_ptr<ConnectionAttempt<BleInitiatorFailureType>>
BleInitiatorConnectionAttempt::Factory::BuildInstance(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details) {
  return base::WrapUnique(new BleInitiatorConnectionAttempt(
      ble_connection_manager, delegate, connection_attempt_details));
}

BleInitiatorConnectionAttempt::BleInitiatorConnectionAttempt(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details)
    : ConnectionAttemptBase<BleInitiatorFailureType>(
          delegate,
          connection_attempt_details),
      ble_connection_manager_(ble_connection_manager) {}

BleInitiatorConnectionAttempt::~BleInitiatorConnectionAttempt() = default;

std::unique_ptr<ConnectToDeviceOperation<BleInitiatorFailureType>>
BleInitiatorConnectionAttempt::CreateConnectToDeviceOperation(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectToDeviceOperation<BleInitiatorFailureType>::ConnectionSuccessCallback
        success_callback,
    const ConnectToDeviceOperation<
        BleInitiatorFailureType>::ConnectionFailedCallback& failure_callback) {
  return BleInitiatorOperation::Factory::Get()->BuildInstance(
      ble_connection_manager_, std::move(success_callback), failure_callback,
      device_id_pair, connection_priority);
}

}  // namespace secure_channel

}  // namespace chromeos
