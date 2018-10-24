// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_forward.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/scheduler_helper.h"

namespace blink {
namespace scheduler {

CompositorThreadScheduler::CompositorThreadScheduler(
    base::Thread* thread,
    std::unique_ptr<base::sequence_manager::SequenceManager> task_queue_manager)
    : NonMainThreadSchedulerImpl(std::make_unique<NonMainThreadSchedulerHelper>(
          std::move(task_queue_manager),
          this,
          TaskType::kCompositorThreadTaskQueueDefault)),
      thread_(thread) {}

CompositorThreadScheduler::~CompositorThreadScheduler() = default;

scoped_refptr<NonMainThreadTaskQueue>
CompositorThreadScheduler::DefaultTaskQueue() {
  return helper()->DefaultNonMainThreadTaskQueue();
}

void CompositorThreadScheduler::InitImpl() {}

void CompositorThreadScheduler::OnTaskCompleted(
    NonMainThreadTaskQueue* worker_task_queue,
    const base::sequence_manager::TaskQueue::Task& task,
    base::TimeTicks start,
    base::TimeTicks end,
    base::Optional<base::TimeDelta> thread_time) {
  compositor_metrics_helper_.RecordTaskMetrics(worker_task_queue, task, start,
                                               end, thread_time);
}

scoped_refptr<scheduler::SingleThreadIdleTaskRunner>
CompositorThreadScheduler::IdleTaskRunner() {
  // TODO(flackr): This posts idle tasks as regular tasks. We need to create
  // an idle task runner with the semantics we want for the compositor thread
  // which runs them after the current frame has been drawn before the next
  // vsync. https://crbug.com/609532
  return base::MakeRefCounted<SingleThreadIdleTaskRunner>(
      thread_->task_runner(), this);
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadScheduler::V8TaskRunner() {
  NOTREACHED();
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
CompositorThreadScheduler::CompositorTaskRunner() {
  NOTREACHED();
  return nullptr;
}

bool CompositorThreadScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

bool CompositorThreadScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

void CompositorThreadScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper()->AddTaskObserver(task_observer);
}

void CompositorThreadScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  helper()->RemoveTaskObserver(task_observer);
}

void CompositorThreadScheduler::Shutdown() {}

void CompositorThreadScheduler::OnIdleTaskPosted() {}

base::TimeTicks CompositorThreadScheduler::WillProcessIdleTask() {
  // TODO(flackr): Return the next frame time as the deadline instead.
  // TODO(flackr): Ensure that oilpan GC does happen on the compositor thread
  // even though we will have no long idle periods. https://crbug.com/609531
  return base::TimeTicks::Now() + base::TimeDelta::FromMillisecondsD(16.7);
}

void CompositorThreadScheduler::DidProcessIdleTask() {}

base::TimeTicks CompositorThreadScheduler::NowTicks() {
  return base::TimeTicks::Now();
}

}  // namespace scheduler
}  // namespace blink
