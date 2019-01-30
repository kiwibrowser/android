// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/nacl_service.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "content/public/common/service_names.mojom.h"
#include "ipc/ipc.mojom.h"
#include "mojo/edk/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"

#if defined(OS_POSIX)
#include "base/files/scoped_file.h"
#include "base/posix/global_descriptors.h"
#include "services/service_manager/embedder/descriptors.h"
#endif

namespace {

mojo::IncomingInvitation EstablishMojoConnection() {
  mojo::PlatformChannelEndpoint endpoint;
#if defined(OS_WIN)
  endpoint = mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
      *base::CommandLine::ForCurrentProcess());
#else
  endpoint = mojo::PlatformChannelEndpoint(mojo::PlatformHandle(
      base::ScopedFD(base::GlobalDescriptors::GetInstance()->Get(
          service_manager::kMojoIPCChannel))));
#endif
  DCHECK(endpoint.is_valid());
  return mojo::IncomingInvitation::Accept(std::move(endpoint));
}

service_manager::mojom::ServiceRequest ConnectToServiceManager(
    mojo::IncomingInvitation* invitation) {
  const std::string service_request_channel_token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          service_manager::switches::kServiceRequestChannelToken);
  DCHECK(!service_request_channel_token.empty());
  mojo::ScopedMessagePipeHandle parent_handle =
      invitation->ExtractMessagePipe(service_request_channel_token);
  DCHECK(parent_handle.is_valid());
  return service_manager::mojom::ServiceRequest(std::move(parent_handle));
}

class NaClService : public service_manager::Service {
 public:
  NaClService(IPC::mojom::ChannelBootstrapPtrInfo bootstrap,
              std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support);
  ~NaClService() override;

  // Service overrides.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

 private:
  IPC::mojom::ChannelBootstrapPtrInfo ipc_channel_bootstrap_;
  std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support_;
  bool connected_ = false;
};

NaClService::NaClService(
    IPC::mojom::ChannelBootstrapPtrInfo bootstrap,
    std::unique_ptr<mojo::edk::ScopedIPCSupport> ipc_support)
    : ipc_channel_bootstrap_(std::move(bootstrap)),
      ipc_support_(std::move(ipc_support)) {}

NaClService::~NaClService() = default;

void NaClService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  if (source_info.identity.name() == content::mojom::kBrowserServiceName &&
      interface_name == IPC::mojom::ChannelBootstrap::Name_ && !connected_) {
    connected_ = true;
    mojo::FuseInterface(
        IPC::mojom::ChannelBootstrapRequest(std::move(interface_pipe)),
        std::move(ipc_channel_bootstrap_));
  } else {
    DVLOG(1) << "Ignoring request for unknown interface " << interface_name;
  }
}

}  // namespace

std::unique_ptr<service_manager::ServiceContext> CreateNaClServiceContext(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojo::ScopedMessagePipeHandle* ipc_channel) {
  auto ipc_support = std::make_unique<mojo::edk::ScopedIPCSupport>(
      std::move(io_task_runner),
      mojo::edk::ScopedIPCSupport::ShutdownPolicy::FAST);
  auto invitation = EstablishMojoConnection();
  IPC::mojom::ChannelBootstrapPtr bootstrap;
  *ipc_channel = mojo::MakeRequest(&bootstrap).PassMessagePipe();
  auto context = std::make_unique<service_manager::ServiceContext>(
      std::make_unique<NaClService>(bootstrap.PassInterface(),
                                    std::move(ipc_support)),
      ConnectToServiceManager(&invitation));
  return context;
}
