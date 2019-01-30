// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/fake_client_connection_parameters.h"

namespace chromeos {

namespace secure_channel {

FakeClientConnectionParameters::FakeClientConnectionParameters(
    const std::string& feature,
    base::OnceCallback<void(const base::UnguessableToken&)> destructor_callback)
    : ClientConnectionParameters(feature),
      destructor_callback_(std::move(destructor_callback)),
      weak_ptr_factory_(this) {}

FakeClientConnectionParameters::~FakeClientConnectionParameters() {
  if (destructor_callback_)
    std::move(destructor_callback_).Run(id());
}

void FakeClientConnectionParameters::CancelClientRequest() {
  DCHECK(!has_canceled_client_request_);
  has_canceled_client_request_ = true;
  NotifyConnectionRequestCanceled();
}

bool FakeClientConnectionParameters::HasClientCanceledRequest() {
  return has_canceled_client_request_;
}

void FakeClientConnectionParameters::PerformSetConnectionAttemptFailed(
    mojom::ConnectionAttemptFailureReason reason) {
  failure_reason_ = reason;
}

void FakeClientConnectionParameters::PerformSetConnectionSucceeded(
    mojom::ChannelPtr channel,
    mojom::MessageReceiverRequest message_receiver_request) {
  DCHECK(message_receiver_);
  DCHECK(!message_receiver_binding_);

  channel_ = std::move(channel);
  channel_->set_connection_error_with_reason_handler(
      base::BindOnce(&FakeClientConnectionParameters::OnChannelDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));

  message_receiver_binding_ =
      std::make_unique<mojo::Binding<mojom::MessageReceiver>>(
          message_receiver_.get(), std::move(message_receiver_request));
}

void FakeClientConnectionParameters::OnChannelDisconnected(
    uint32_t disconnection_reason,
    const std::string& disconnection_description) {
  disconnection_reason_ = disconnection_reason;
  channel_.reset();
}

FakeClientConnectionParametersObserver::
    FakeClientConnectionParametersObserver() = default;

FakeClientConnectionParametersObserver::
    ~FakeClientConnectionParametersObserver() = default;

void FakeClientConnectionParametersObserver::OnConnectionRequestCanceled() {
  has_connection_request_been_canceled_ = true;

  if (closure_for_next_callback_)
    std::move(closure_for_next_callback_).Run();
}

}  // namespace secure_channel

}  // namespace chromeos
