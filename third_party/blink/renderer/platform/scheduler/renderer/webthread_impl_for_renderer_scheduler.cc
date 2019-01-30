// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/renderer/webthread_impl_for_renderer_scheduler.h"

#include "base/location.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_forward.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

namespace blink {
namespace scheduler {

WebThreadImplForRendererScheduler::WebThreadImplForRendererScheduler(
    MainThreadSchedulerImpl* scheduler)
    : task_runner_(scheduler->DefaultTaskRunner()),
      scheduler_(scheduler),
      thread_id_(base::PlatformThread::CurrentId()) {}

WebThreadImplForRendererScheduler::~WebThreadImplForRendererScheduler() =
    default;

blink::PlatformThreadId WebThreadImplForRendererScheduler::ThreadId() const {
  return thread_id_;
}

blink::ThreadScheduler* WebThreadImplForRendererScheduler::Scheduler() const {
  return scheduler_;
}

scoped_refptr<base::SingleThreadTaskRunner>
WebThreadImplForRendererScheduler::GetTaskRunner() const {
  return task_runner_;
}

void WebThreadImplForRendererScheduler::AddTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->AddTaskObserver(observer);
}

void WebThreadImplForRendererScheduler::RemoveTaskObserverInternal(
    base::MessageLoop::TaskObserver* observer) {
  scheduler_->RemoveTaskObserver(observer);
}

void WebThreadImplForRendererScheduler::AddTaskTimeObserverInternal(
    base::sequence_manager::TaskTimeObserver* task_time_observer) {
  scheduler_->AddTaskTimeObserver(task_time_observer);
}

void WebThreadImplForRendererScheduler::RemoveTaskTimeObserverInternal(
    base::sequence_manager::TaskTimeObserver* task_time_observer) {
  scheduler_->RemoveTaskTimeObserver(task_time_observer);
}

void WebThreadImplForRendererScheduler::Init() {}

}  // namespace scheduler
}  // namespace blink
