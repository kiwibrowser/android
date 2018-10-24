// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/context_provider/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <zircon/processargs.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_zx_handle.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "webrunner/app/switches.h"

namespace fuchsia {
namespace {

// Relaunches the current executable as a Context process.
base::Process LaunchContextProcess(const base::LaunchOptions& launch_options) {
  base::CommandLine launch_command = *base::CommandLine::ForCurrentProcess();
  DCHECK(!launch_command.HasSwitch(kContextProcess));
  launch_command.AppendSwitch(kContextProcess);
  return base::LaunchProcess(launch_command, launch_options);
}

}  // namespace

ContextProviderImpl::ContextProviderImpl()
    : launch_(base::BindRepeating(&LaunchContextProcess)) {}

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::SetLaunchCallbackForTests(
    const LaunchContextProcessCallback& launch) {
  launch_ = launch;
}

void ContextProviderImpl::Create(
    chromium::web::CreateContextParams params,
    ::fidl::InterfaceRequest<chromium::web::Context> context_request) {
  DCHECK(context_request.is_valid());

  if (params.dataDirectory) {
    // TODO(https://crbug.com/850743): Implement this.
    NOTIMPLEMENTED()
        << "Persistent data directory binding is not yet implemented.";
  }

  // Transfer the ContextRequest handle to a well-known location in the child
  // process' handle table.
  base::LaunchOptions launch_options;
  base::ScopedZxHandle context_handle(context_request.TakeChannel().release());
  launch_options.handles_to_transfer.push_back(
      {PA_HND(PA_USER0, 0), context_handle.get()});

  // Isolate the child Context processes by containing them within their own
  // respective jobs.
  base::ScopedZxHandle job;
  zx_status_t status = zx_job_create(base::GetDefaultJob(), 0, job.receive());
  ZX_CHECK(status == ZX_OK, status) << "zx_job_create";

  ignore_result(launch_.Run(launch_options));
  ignore_result(context_handle.release());
  ignore_result(job.release());
}

void ContextProviderImpl::Bind(
    fidl::InterfaceRequest<chromium::web::ContextProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace fuchsia
