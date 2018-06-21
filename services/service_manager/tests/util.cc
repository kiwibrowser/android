// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/tests/util.h"

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/service_manager/runner/common/switches.h"

namespace service_manager {
namespace test {

namespace {

void GrabConnectResult(base::RunLoop* loop,
                       mojom::ConnectResult* out_result,
                       mojom::ConnectResult result,
                       const Identity& resolved_identity) {
  loop->Quit();
  *out_result = result;
}

}  // namespace

mojom::ConnectResult LaunchAndConnectToProcess(
    const std::string& target_exe_name,
    const Identity& target,
    service_manager::Connector* connector,
    base::Process* process) {
  base::FilePath target_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &target_path));
  target_path = target_path.AppendASCII(target_exe_name);

  base::CommandLine child_command_line(target_path);
  // Forward the wait-for-debugger flag but nothing else - we don't want to
  // stamp on the platform-channel flag.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kWaitForDebugger)) {
    child_command_line.AppendSwitch(::switches::kWaitForDebugger);
  }

  // Create the channel to be shared with the target process. Pass one end
  // on the command line.
  mojo::PlatformChannel channel;
  mojo::PlatformChannel::HandlePassingInfo handle_passing_info;
  channel.PrepareToPassRemoteEndpoint(&handle_passing_info,
                                      &child_command_line);

  mojo::OutgoingInvitation invitation;
  auto pipe_name = base::NumberToString(base::RandUint64());
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(pipe_name);
  child_command_line.AppendSwitchASCII(switches::kServicePipeToken, pipe_name);

  service_manager::mojom::ServicePtr client;
  client.Bind(mojo::InterfacePtrInfo<service_manager::mojom::Service>(
      std::move(pipe), 0u));
  service_manager::mojom::PIDReceiverPtr receiver;

  connector->StartService(target, std::move(client), MakeRequest(&receiver));
  mojom::ConnectResult result;
  {
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    Connector::TestApi test_api(connector);
    test_api.SetStartServiceCallback(
        base::Bind(&GrabConnectResult, &loop, &result));
    loop.Run();
  }

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.handles_to_inherit = handle_passing_info;
#elif defined(OS_FUCHSIA)

  options.handles_to_transfer = handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = handle_passing_info;
#endif
  *process = base::LaunchProcess(child_command_line, options);
  DCHECK(process->IsValid());
  channel.RemoteProcessLaunchAttempted();
  receiver->SetPID(process->Pid());
  mojo::OutgoingInvitation::Send(std::move(invitation), process->Handle(),
                                 channel.TakeLocalEndpoint());
  return result;
}

}  // namespace test
}  // namespace service_manager
