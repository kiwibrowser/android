// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_task_queue.h"

#include "third_party/blink/renderer/platform/scheduler/base/task_queue_impl_forward.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

NonMainThreadTaskQueue::NonMainThreadTaskQueue(
    std::unique_ptr<base::sequence_manager::internal::TaskQueueImpl> impl,
    const TaskQueue::Spec& spec,
    NonMainThreadSchedulerImpl* non_main_thread_scheduler)
    : TaskQueue(std::move(impl), spec),
      non_main_thread_scheduler_(non_main_thread_scheduler) {
  if (GetTaskQueueImpl()) {
    // TaskQueueImpl may be null for tests.
    GetTaskQueueImpl()->SetOnTaskCompletedHandler(base::BindRepeating(
        &NonMainThreadTaskQueue::OnTaskCompleted, base::Unretained(this)));
  }
}

NonMainThreadTaskQueue::~NonMainThreadTaskQueue() = default;

void NonMainThreadTaskQueue::OnTaskCompleted(
    const TaskQueue::Task& task,
    base::TimeTicks start,
    base::TimeTicks end,
    base::Optional<base::TimeDelta> thread_time) {
  // |non_main_thread_scheduler_| can be nullptr in tests.
  if (non_main_thread_scheduler_) {
    non_main_thread_scheduler_->OnTaskCompleted(this, task, start, end,
                                                thread_time);
  }
}

}  // namespace scheduler
}  // namespace blink
