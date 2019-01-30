// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/client_connection_parameters_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

// static
ClientConnectionParametersImpl::Factory*
    ClientConnectionParametersImpl::Factory::test_factory_ = nullptr;

// static
ClientConnectionParametersImpl::Factory*
ClientConnectionParametersImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void ClientConnectionParametersImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ClientConnectionParametersImpl::Factory::~Factory() = default;

std::unique_ptr<ClientConnectionParameters>
ClientConnectionParametersImpl::Factory::BuildInstance(
    const std::string& feature,
    mojom::ConnectionDelegatePtr connection_delegate_ptr) {
  return base::WrapUnique(new ClientConnectionParametersImpl(
      feature, std::move(connection_delegate_ptr)));
}

ClientConnectionParametersImpl::ClientConnectionParametersImpl(
    const std::string& feature,
    mojom::ConnectionDelegatePtr connection_delegate_ptr)
    : ClientConnectionParameters(feature),
      connection_delegate_ptr_(std::move(connection_delegate_ptr)) {
  // If the client disconnects its delegate, the client is signaling that the
  // connection request has been canceled.
  connection_delegate_ptr_.set_connection_error_handler(base::BindOnce(
      &ClientConnectionParametersImpl::OnConnectionDelegatePtrDisconnected,
      base::Unretained(this)));
}

ClientConnectionParametersImpl::~ClientConnectionParametersImpl() = default;

bool ClientConnectionParametersImpl::HasClientCanceledRequest() {
  return connection_delegate_ptr_.encountered_error();
}

void ClientConnectionParametersImpl::PerformSetConnectionAttemptFailed(
    mojom::ConnectionAttemptFailureReason reason) {
  connection_delegate_ptr_->OnConnectionAttemptFailure(reason);
}

void ClientConnectionParametersImpl::PerformSetConnectionSucceeded(
    mojom::ChannelPtr channel,
    mojom::MessageReceiverRequest message_receiver_request) {
  connection_delegate_ptr_->OnConnection(std::move(channel),
                                         std::move(message_receiver_request));
}

void ClientConnectionParametersImpl::OnConnectionDelegatePtrDisconnected() {
  NotifyConnectionRequestCanceled();
}

}  // namespace secure_channel

}  // namespace chromeos
