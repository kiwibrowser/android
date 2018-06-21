// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace chromeos {

namespace secure_channel {

// Provides clients access to the SecureChannel API.
class SecureChannelClientImpl : public SecureChannelClient {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetInstanceForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<SecureChannelClient> BuildInstance(
        service_manager::Connector* connector,
        scoped_refptr<base::TaskRunner> task_runner =
            base::ThreadTaskRunnerHandle::Get());

   private:
    static Factory* test_factory_;
  };

  ~SecureChannelClientImpl() override;

 private:
  friend class SecureChannelClientImplTest;

  SecureChannelClientImpl(service_manager::Connector* connector,
                          scoped_refptr<base::TaskRunner> task_runner);

  // SecureChannelClient:
  std::unique_ptr<ConnectionAttempt> InitiateConnectionToDevice(
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) override;
  std::unique_ptr<ConnectionAttempt> ListenForConnectionFromDevice(
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority) override;

  void PerformInitiateConnectionToDevice(
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionDelegatePtr connection_delegate_ptr);
  void PerformListenForConnectionFromDevice(
      cryptauth::RemoteDeviceRef device_to_connect,
      cryptauth::RemoteDeviceRef local_device,
      const std::string& feature,
      ConnectionPriority connection_priority,
      mojom::ConnectionDelegatePtr connection_delegate_ptr);

  void FlushForTesting();

  mojom::SecureChannelPtr secure_channel_ptr_;

  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<SecureChannelClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelClientImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_SECURE_CHANNEL_CLIENT_IMPL_H_
