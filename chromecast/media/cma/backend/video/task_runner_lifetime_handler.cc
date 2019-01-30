// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video/task_runner_lifetime_handler.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/base/task_runner_impl.h"

namespace chromecast {
namespace media {

base::AtExitManager g_at_exit_manager;
std::unique_ptr<base::ThreadTaskRunnerHandle> g_thread_task_runner_handle;

// static
void TaskRunnerLifetimeHandler::SetTaskRunnerHandle(
    const TaskRunnerImpl* task_runner_in) {
  // Set up the static reference in base::ThreadTaskRunnerHandle::Get
  // for the media thread in this shared library.  We can extract the
  // SingleThreadTaskRunner passed in from cast_shell for this.
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    DCHECK(!g_thread_task_runner_handle);
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        task_runner_in->runner();
    DCHECK(task_runner->BelongsToCurrentThread());
    g_thread_task_runner_handle.reset(
        new base::ThreadTaskRunnerHandle(task_runner));
  }
}

// static
void TaskRunnerLifetimeHandler::ResetTaskRunnerHandle() {
  g_thread_task_runner_handle.reset();
}

}  // namespace media
}  // namespace chromecast
