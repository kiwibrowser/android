// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"

#include "base/no_destructor.h"
#include "base/task_runner.h"
#include "chromeos/services/secure_channel/public/cpp/client/connection_attempt_impl.h"
#include "chromeos/services/secure_channel/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {

namespace secure_channel {

// static
SecureChannelClientImpl::Factory*
    SecureChannelClientImpl::Factory::test_factory_ = nullptr;

// static
SecureChannelClientImpl::Factory* SecureChannelClientImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void SecureChannelClientImpl::Factory::SetInstanceForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelClientImpl::Factory::~Factory() = default;

std::unique_ptr<SecureChannelClient>
SecureChannelClientImpl::Factory::BuildInstance(
    service_manager::Connector* connector,
    scoped_refptr<base::TaskRunner> task_runner) {
  return base::WrapUnique(new SecureChannelClientImpl(connector, task_runner));
}

SecureChannelClientImpl::SecureChannelClientImpl(
    service_manager::Connector* connector,
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  connector->BindInterface(mojom::kServiceName, &secure_channel_ptr_);
}

SecureChannelClientImpl::~SecureChannelClientImpl() = default;

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::InitiateConnectionToDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  auto connection_attempt =
      ConnectionAttemptImpl::Factory::Get()->BuildInstance();

  // Delay directly calling mojom::SecureChannel::InitiateConnectionToDevice()
  // until the caller has added itself as a Delegate of the ConnectionAttempt.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SecureChannelClientImpl::PerformInitiateConnectionToDevice,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect, local_device,
          feature, connection_priority,
          connection_attempt->GenerateInterfacePtr()));

  return connection_attempt;
}

std::unique_ptr<ConnectionAttempt>
SecureChannelClientImpl::ListenForConnectionFromDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority) {
  auto connection_attempt =
      ConnectionAttemptImpl::Factory::Get()->BuildInstance();

  // Delay directly calling
  // mojom::SecureChannel::ListenForConnectionFromDevice() until the caller has
  // added itself as a Delegate of the ConnectionAttempt.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SecureChannelClientImpl::PerformListenForConnectionFromDevice,
          weak_ptr_factory_.GetWeakPtr(), device_to_connect, local_device,
          feature, connection_priority,
          connection_attempt->GenerateInterfacePtr()));

  return connection_attempt;
}

void SecureChannelClientImpl::PerformInitiateConnectionToDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojom::ConnectionDelegatePtr connection_delegate_ptr) {
  secure_channel_ptr_->InitiateConnectionToDevice(
      *device_to_connect.remote_device_, *local_device.remote_device_, feature,
      connection_priority, std::move(connection_delegate_ptr));
}

void SecureChannelClientImpl::PerformListenForConnectionFromDevice(
    cryptauth::RemoteDeviceRef device_to_connect,
    cryptauth::RemoteDeviceRef local_device,
    const std::string& feature,
    ConnectionPriority connection_priority,
    mojom::ConnectionDelegatePtr connection_delegate_ptr) {
  secure_channel_ptr_->ListenForConnectionFromDevice(
      *device_to_connect.remote_device_, *local_device.remote_device_, feature,
      connection_priority, std::move(connection_delegate_ptr));
}

void SecureChannelClientImpl::FlushForTesting() {
  secure_channel_ptr_.FlushForTesting();
}

}  // namespace secure_channel

}  // namespace chromeos
