// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/prioritized_task_runner.h"

#include <algorithm>

#include "base/bind.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"

namespace net {

PrioritizedTaskRunner::Job::Job(const base::Location& from_here,
                                base::OnceClosure task,
                                base::OnceClosure reply,
                                uint32_t priority,
                                uint32_t task_count)
    : from_here(from_here),
      task(std::move(task)),
      reply(std::move(reply)),
      priority(priority),
      task_count(task_count) {}

PrioritizedTaskRunner::Job::Job() {}

PrioritizedTaskRunner::Job::~Job() = default;
PrioritizedTaskRunner::Job::Job(Job&& other) = default;
PrioritizedTaskRunner::Job& PrioritizedTaskRunner::Job::operator=(Job&& other) =
    default;

PrioritizedTaskRunner::PrioritizedTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

void PrioritizedTaskRunner::PostTaskAndReply(const base::Location& from_here,
                                             base::OnceClosure task,
                                             base::OnceClosure reply,
                                             uint32_t priority) {
  Job job(from_here, std::move(task), std::move(reply), priority,
          task_count_++);
  {
    base::AutoLock lock(job_heap_lock_);
    job_heap_.push_back(std::move(job));
    std::push_heap(job_heap_.begin(), job_heap_.end(), JobComparer());
  }

  Job* out_job = new Job();

  task_runner_->PostTaskAndReply(
      from_here,
      base::BindOnce(&PrioritizedTaskRunner::RunPostTaskAndReply, this,
                     out_job),
      base::BindOnce(&PrioritizedTaskRunner::RunReply, this,
                     base::Owned(out_job)));
}

PrioritizedTaskRunner::Job PrioritizedTaskRunner::PopJob() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock lock(job_heap_lock_);
  std::pop_heap(job_heap_.begin(), job_heap_.end(), JobComparer());
  Job result = std::move(job_heap_.back());
  job_heap_.pop_back();
  return result;
}

PrioritizedTaskRunner::~PrioritizedTaskRunner() = default;

void PrioritizedTaskRunner::RunPostTaskAndReply(Job* out_job) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  *out_job = PopJob();
  std::move(out_job->task).Run();
}

void PrioritizedTaskRunner::RunReply(Job* job) {
  std::move(job->reply).Run();
}

}  // namespace net
