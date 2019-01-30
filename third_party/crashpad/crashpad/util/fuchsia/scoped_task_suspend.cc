// Copyright 2018 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/fuchsia/scoped_task_suspend.h"

#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>

#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_zx_handle.h"
#include "base/logging.h"
#include "util/fuchsia/koid_utilities.h"

namespace crashpad {

namespace {

zx_obj_type_t GetHandleType(zx_handle_t handle) {
  zx_info_handle_basic_t basic;
  zx_status_t status = zx_object_get_info(
      handle, ZX_INFO_HANDLE_BASIC, &basic, sizeof(basic), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return ZX_OBJ_TYPE_NONE;
  }
  return basic.type;
}

// Returns the suspend token of the suspended thread. This function attempts
// to wait a short time for the thread to actually suspend before returning
// but this is not guaranteed.
base::ScopedZxHandle SuspendThread(zx_handle_t thread) {
  zx_handle_t token = ZX_HANDLE_INVALID;
  zx_status_t status = zx_task_suspend_token(thread, &token);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_task_suspend";
    base::ScopedZxHandle();
  }

  zx_signals_t observed = 0u;
  if (zx_object_wait_one(thread, ZX_THREAD_SUSPENDED,
                         zx_deadline_after(ZX_MSEC(50)), &observed) != ZX_OK) {
    LOG(ERROR) << "thread failed to suspend";
  }
  return base::ScopedZxHandle(token);
}

}  // namespace

ScopedTaskSuspend::ScopedTaskSuspend(zx_handle_t task) {
  DCHECK_NE(task, zx_process_self());
  DCHECK_NE(task, zx_thread_self());

  zx_obj_type_t type = GetHandleType(task);
  if (type == ZX_OBJ_TYPE_THREAD) {
    suspend_tokens_.push_back(SuspendThread(task));
  } else if (type == ZX_OBJ_TYPE_PROCESS) {
    for (const auto& thread : GetChildHandles(task, ZX_INFO_PROCESS_THREADS))
      suspend_tokens_.push_back(SuspendThread(thread.get()));
  } else {
    LOG(ERROR) << "unexpected handle type";
  }
}

ScopedTaskSuspend::~ScopedTaskSuspend() = default;

}  // namespace crashpad
