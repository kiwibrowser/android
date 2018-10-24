// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_EMBEDDER_H_
#define MOJO_EDK_EMBEDDER_EMBEDDER_H_

#include <stddef.h>

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "base/process/process_handle.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/configuration.h"

namespace base {
class PortProvider;
}

namespace mojo {
namespace edk {

using ProcessErrorCallback = base::Callback<void(const std::string& error)>;

// Basic configuration/initialization ------------------------------------------

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system state. There is no corresponding
// shutdown operation: once the EDK is initialized, public Mojo C API calls
// remain available for the remainder of the process's lifetime.
COMPONENT_EXPORT(MOJO_EDK_EMBEDDER)
void Init(const Configuration& configuration);

// Like above but uses a default Configuration.
COMPONENT_EXPORT(MOJO_EDK_EMBEDDER) void Init();

// Sets a default callback to invoke when an internal error is reported but
// cannot be associated with a specific child process. Calling this is optional.
COMPONENT_EXPORT(MOJO_EDK_EMBEDDER)
void SetDefaultProcessErrorCallback(const ProcessErrorCallback& callback);

// Initialialization/shutdown for interprocess communication (IPC) -------------

// Retrieves the TaskRunner used for IPC I/O, as set by ScopedIPCSupport.
COMPONENT_EXPORT(MOJO_EDK_EMBEDDER)
scoped_refptr<base::TaskRunner> GetIOTaskRunner();

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Set the |base::PortProvider| for this process. Can be called on any thread,
// but must be set in the root process before any Mach ports can be transferred.
//
// If called at all, this must be called while a ScopedIPCSupport exists.
COMPONENT_EXPORT(MOJO_EDK_EMBEDDER)
void SetMachPortProvider(base::PortProvider* port_provider);
#endif

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_H_
