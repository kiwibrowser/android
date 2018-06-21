// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_SERVICE_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_SERVICE_H_

#include <memory>
#include <string>

#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelBase;

// Service which provides an implementation for
// secure_channel::mojom::SecureChannel. This service creates one
// implementation and shares it among all connection requests.
class SecureChannelService : public service_manager::Service {
 public:
  static std::unique_ptr<service_manager::Service> CreateService();

  SecureChannelService();
  ~SecureChannelService() override;

 protected:
  // service_manager::Service:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  std::unique_ptr<SecureChannelBase> secure_channel_;
  service_manager::BinderRegistry registry_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelService);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_SECURE_CHANNEL_SERVICE_H_
