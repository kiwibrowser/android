// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/embedder/embedder.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/entrypoints.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/public/c/system/thunks.h"

namespace mojo {
namespace edk {

void Init(const Configuration& configuration) {
  internal::g_configuration = configuration;
  InitializeCore();
  MojoEmbedderSetSystemThunks(&GetSystemThunks());
}

void Init() {
  Init(Configuration());
}

void SetDefaultProcessErrorCallback(const ProcessErrorCallback& callback) {
  Core::Get()->SetDefaultProcessErrorCallback(callback);
}

scoped_refptr<base::TaskRunner> GetIOTaskRunner() {
  return Core::Get()->GetNodeController()->io_task_runner();
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void SetMachPortProvider(base::PortProvider* port_provider) {
  DCHECK(port_provider);
  Core::Get()->SetMachPortProvider(port_provider);
}
#endif

}  // namespace edk
}  // namespace mojo
