// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_listener_connection_attempt.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/services/secure_channel/ble_listener_operation.h"

namespace chromeos {

namespace secure_channel {

// static
BleListenerConnectionAttempt::Factory*
    BleListenerConnectionAttempt::Factory::test_factory_ = nullptr;

// static
BleListenerConnectionAttempt::Factory*
BleListenerConnectionAttempt::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleListenerConnectionAttempt::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleListenerConnectionAttempt::Factory::~Factory() = default;

std::unique_ptr<ConnectionAttempt<BleListenerFailureType>>
BleListenerConnectionAttempt::Factory::BuildInstance(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details) {
  return base::WrapUnique(new BleListenerConnectionAttempt(
      ble_connection_manager, delegate, connection_attempt_details));
}

BleListenerConnectionAttempt::BleListenerConnectionAttempt(
    BleConnectionManager* ble_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details)
    : ConnectionAttemptBase<BleListenerFailureType>(delegate,
                                                    connection_attempt_details),
      ble_connection_manager_(ble_connection_manager) {}

BleListenerConnectionAttempt::~BleListenerConnectionAttempt() = default;

std::unique_ptr<ConnectToDeviceOperation<BleListenerFailureType>>
BleListenerConnectionAttempt::CreateConnectToDeviceOperation(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectToDeviceOperation<BleListenerFailureType>::ConnectionSuccessCallback
        success_callback,
    const ConnectToDeviceOperation<
        BleListenerFailureType>::ConnectionFailedCallback& failure_callback) {
  return BleListenerOperation::Factory::Get()->BuildInstance(
      ble_connection_manager_, std::move(success_callback), failure_callback,
      device_id_pair, connection_priority);
}

}  // namespace secure_channel

}  // namespace chromeos
