// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/secure_channel_service.h"

#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/secure_channel_initializer.h"

namespace chromeos {

namespace secure_channel {

// static
std::unique_ptr<service_manager::Service>
SecureChannelService::CreateService() {
  return std::make_unique<SecureChannelService>();
}

SecureChannelService::SecureChannelService() = default;

SecureChannelService::~SecureChannelService() = default;

void SecureChannelService::OnStart() {
  PA_LOG(INFO) << "SecureChannelService::OnStart()";

  secure_channel_ = SecureChannelInitializer::Factory::Get()->BuildInstance();

  registry_.AddInterface(
      base::BindRepeating(&SecureChannelBase::BindRequest,
                          base::Unretained(secure_channel_.get())));
}

void SecureChannelService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  PA_LOG(INFO) << "SecureChannelService::OnBindInterface() for interface "
               << interface_name << ".";
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace secure_channel

}  // namespace chromeos
