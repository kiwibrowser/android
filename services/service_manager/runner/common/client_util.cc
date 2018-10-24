// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/common/client_util.h"

#include <string>

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/runner/common/switches.h"

namespace service_manager {

mojom::ServicePtr PassServiceRequestOnCommandLine(
    mojo::OutgoingInvitation* invitation,
    base::CommandLine* command_line) {
  mojom::ServicePtr client;
  auto pipe_name = base::NumberToString(base::RandUint64());
  client.Bind(
      mojom::ServicePtrInfo(invitation->AttachMessagePipe(pipe_name), 0));
  command_line->AppendSwitchASCII(switches::kServicePipeToken, pipe_name);
  return client;
}

mojom::ServiceRequest GetServiceRequestFromCommandLine(
    mojo::IncomingInvitation* invitation) {
  std::string pipe_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kServicePipeToken);
  return mojom::ServiceRequest(invitation->ExtractMessagePipe(pipe_name));
}

bool ServiceManagerIsRemote() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kServicePipeToken);
}

}  // namespace service_manager
