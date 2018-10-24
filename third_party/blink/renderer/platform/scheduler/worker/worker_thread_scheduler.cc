// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/scheduler/base/sequence_manager_forward.h"
#include "third_party/blink/renderer/platform/scheduler/base/task_queue_forward.h"
#include "third_party/blink/renderer/platform/scheduler/child/features.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/task_queue_throttler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_scheduler_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

using base::sequence_manager::TaskQueue;

namespace {
// Workers could be short-lived, set a shorter interval than
// the renderer thread.
constexpr base::TimeDelta kUnspecifiedWorkerThreadLoadTrackerReportingInterval =
    base::TimeDelta::FromSeconds(1);

// Worker throttling trial
const char kWorkerThrottlingTrial[] = "BlinkSchedulerDedicatedWorkerThrottling";
const char kWorkerThrottlingMaxBudgetParam[] = "max_budget_ms";
const char kWorkerThrottlingRecoveryRateParam[] = "recovery_rate";
const char kWorkerThrottlingMaxDelayParam[] = "max_delay_ms";

constexpr base::TimeDelta kDefaultMaxBudget = base::TimeDelta::FromSeconds(1);
constexpr double kDefaultRecoveryRate = 0.01;
constexpr base::TimeDelta kDefaultMaxThrottlingDelay =
    base::TimeDelta::FromSeconds(60);

void ReportWorkerTaskLoad(base::TimeTicks time, double load) {
  int load_percentage = static_cast<int>(load * 100);
  DCHECK_LE(load_percentage, 100);
  // TODO(kinuko): Maybe we also want to separately log when the associated
  // tab is in foreground and when not.
  UMA_HISTOGRAM_PERCENTAGE("WorkerScheduler.WorkerThreadLoad", load_percentage);
}

base::Optional<base::TimeDelta> GetMaxBudgetLevel() {
  int max_budget_level_ms;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingMaxBudgetParam),
          &max_budget_level_ms)) {
    return kDefaultMaxBudget;
  }
  if (max_budget_level_ms < 0)
    return base::nullopt;
  return base::TimeDelta::FromMilliseconds(max_budget_level_ms);
}

double GetBudgetRecoveryRate() {
  double recovery_rate;
  if (!base::StringToDouble(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingRecoveryRateParam),
          &recovery_rate)) {
    return kDefaultRecoveryRate;
  }
  return recovery_rate;
}

base::Optional<base::TimeDelta> GetMaxThrottlingDelay() {
  int max_throttling_delay_ms;
  if (!base::StringToInt(
          base::GetFieldTrialParamValue(kWorkerThrottlingTrial,
                                        kWorkerThrottlingMaxDelayParam),
          &max_throttling_delay_ms)) {
    return kDefaultMaxThrottlingDelay;
  }
  if (max_throttling_delay_ms < 0)
    return base::nullopt;
  return base::TimeDelta::FromMilliseconds(max_throttling_delay_ms);
}

}  // namespace

WorkerThreadScheduler::WorkerThreadScheduler(
    WebThreadType thread_type,
    std::unique_ptr<base::sequence_manager::SequenceManager> task_queue_manager,
    WorkerSchedulerProxy* proxy)
    : NonMainThreadSchedulerImpl(std::make_unique<NonMainThreadSchedulerHelper>(
          std::move(task_queue_manager),
          this,
          TaskType::kWorkerThreadTaskQueueDefault)),
      idle_helper_(helper(),
                   this,
                   "WorkerSchedulerIdlePeriod",
                   base::TimeDelta::FromMilliseconds(300),
                   helper()->NewTaskQueue(TaskQueue::Spec("worker_idle_tq"))),
      idle_canceled_delayed_task_sweeper_(helper(),
                                          idle_helper_.IdleTaskRunner()),
      load_tracker_(helper()->NowTicks(),
                    base::BindRepeating(&ReportWorkerTaskLoad),
                    kUnspecifiedWorkerThreadLoadTrackerReportingInterval),
      lifecycle_state_(proxy ? proxy->lifecycle_state()
                             : SchedulingLifecycleState::kNotThrottled),
      worker_metrics_helper_(thread_type) {
  thread_start_time_ = helper()->NowTicks();
  load_tracker_.Resume(thread_start_time_);
  helper()->AddTaskTimeObserver(this);

  if (proxy && proxy->parent_frame_type())
    worker_metrics_helper_.SetParentFrameType(*proxy->parent_frame_type());

  if (thread_type == WebThreadType::kDedicatedWorkerThread &&
      base::FeatureList::IsEnabled(kDedicatedWorkerThrottling)) {
    CreateTaskQueueThrottler();
  }

  TRACE_EVENT_OBJECT_CREATED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);
}

WorkerThreadScheduler::~WorkerThreadScheduler() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("worker.scheduler"), "WorkerScheduler", this);

  helper()->RemoveTaskTimeObserver(this);

  DCHECK(worker_schedulers_.empty());
}

scoped_refptr<SingleThreadIdleTaskRunner>
WorkerThreadScheduler::IdleTaskRunner() {
  DCHECK(initialized_);
  return idle_helper_.IdleTaskRunner();
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerThreadScheduler::V8TaskRunner() {
  DCHECK(initialized_);
  return v8_task_runner_;
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerThreadScheduler::CompositorTaskRunner() {
  DCHECK(initialized_);
  return compositor_task_runner_;
}

bool WorkerThreadScheduler::CanExceedIdleDeadlineIfRequired() const {
  DCHECK(initialized_);
  return idle_helper_.CanExceedIdleDeadlineIfRequired();
}

bool WorkerThreadScheduler::ShouldYieldForHighPriorityWork() {
  // We don't consider any work as being high priority on workers.
  return false;
}

void WorkerThreadScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper()->AddTaskObserver(task_observer);
}

void WorkerThreadScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {
  DCHECK(initialized_);
  helper()->RemoveTaskObserver(task_observer);
}

void WorkerThreadScheduler::Shutdown() {
  DCHECK(initialized_);
  load_tracker_.RecordIdle(helper()->NowTicks());
  base::TimeTicks end_time = helper()->NowTicks();
  base::TimeDelta delta = end_time - thread_start_time_;

  // The lifetime could be radically different for different workers,
  // some workers could be short-lived (but last at least 1 sec in
  // Service Workers case) or could be around as long as the tab is open.
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "WorkerThread.Runtime", delta, base::TimeDelta::FromSeconds(1),
      base::TimeDelta::FromDays(1), 50 /* bucket count */);
  task_queue_throttler_.reset();
  helper()->Shutdown();
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerThreadScheduler::DefaultTaskQueue() {
  DCHECK(initialized_);
  return helper()->DefaultNonMainThreadTaskQueue();
}

void WorkerThreadScheduler::InitImpl() {
  initialized_ = true;
  idle_helper_.EnableLongIdlePeriod();

  v8_task_runner_ = TaskQueueWithTaskType::Create(
      DefaultTaskQueue(), TaskType::kWorkerThreadTaskQueueV8);
  compositor_task_runner_ = TaskQueueWithTaskType::Create(
      DefaultTaskQueue(), TaskType::kWorkerThreadTaskQueueCompositor);
}

void WorkerThreadScheduler::OnTaskCompleted(
    NonMainThreadTaskQueue* worker_task_queue,
    const TaskQueue::Task& task,
    base::TimeTicks start,
    base::TimeTicks end,
    base::Optional<base::TimeDelta> thread_time) {
  worker_metrics_helper_.RecordTaskMetrics(worker_task_queue, task, start, end,
                                           thread_time);

  if (task_queue_throttler_) {
    task_queue_throttler_->OnTaskRunTimeReported(worker_task_queue, start, end);
  }
}

SchedulerHelper* WorkerThreadScheduler::GetSchedulerHelperForTesting() {
  return helper();
}

bool WorkerThreadScheduler::CanEnterLongIdlePeriod(base::TimeTicks,
                                                   base::TimeDelta*) {
  return true;
}

base::TimeTicks WorkerThreadScheduler::CurrentIdleTaskDeadlineForTesting()
    const {
  return idle_helper_.CurrentIdleTaskDeadline();
}

void WorkerThreadScheduler::WillProcessTask(base::TimeTicks start_time) {}

void WorkerThreadScheduler::DidProcessTask(base::TimeTicks start_time,
                                           base::TimeTicks end_time) {
  load_tracker_.RecordTaskTime(start_time, end_time);
}

void WorkerThreadScheduler::OnLifecycleStateChanged(
    SchedulingLifecycleState lifecycle_state) {
  if (lifecycle_state_ == lifecycle_state)
    return;
  lifecycle_state_ = lifecycle_state;

  for (WorkerScheduler* worker_scheduler : worker_schedulers_)
    worker_scheduler->OnLifecycleStateChanged(lifecycle_state);
}

void WorkerThreadScheduler::RegisterWorkerScheduler(
    WorkerScheduler* worker_scheduler) {
  worker_schedulers_.insert(worker_scheduler);
  worker_scheduler->OnLifecycleStateChanged(lifecycle_state_);
}

void WorkerThreadScheduler::UnregisterWorkerScheduler(
    WorkerScheduler* worker_scheduler) {
  DCHECK(worker_schedulers_.find(worker_scheduler) != worker_schedulers_.end());
  worker_schedulers_.erase(worker_scheduler);
}

scoped_refptr<NonMainThreadTaskQueue>
WorkerThreadScheduler::ControlTaskQueue() {
  return helper()->ControlNonMainThreadTaskQueue();
}

void WorkerThreadScheduler::CreateTaskQueueThrottler() {
  if (task_queue_throttler_)
    return;
  task_queue_throttler_ = std::make_unique<TaskQueueThrottler>(
      this, &traceable_variable_controller_);
  wake_up_budget_pool_ =
      task_queue_throttler_->CreateWakeUpBudgetPool("worker_wake_up_pool");
  cpu_time_budget_pool_ =
      task_queue_throttler_->CreateCPUTimeBudgetPool("worker_cpu_time_pool");

  base::TimeTicks now = GetTickClock()->NowTicks();
  cpu_time_budget_pool_->SetMaxBudgetLevel(now, GetMaxBudgetLevel());
  cpu_time_budget_pool_->SetTimeBudgetRecoveryRate(now,
                                                   GetBudgetRecoveryRate());
  cpu_time_budget_pool_->SetMaxThrottlingDelay(now, GetMaxThrottlingDelay());
}

void WorkerThreadScheduler::SetCPUTimeBudgetPoolForTesting(
    CPUTimeBudgetPool* cpu_time_budget_pool) {
  cpu_time_budget_pool_ = cpu_time_budget_pool;
}

std::unordered_set<WorkerScheduler*>&
WorkerThreadScheduler::GetWorkerSchedulersForTesting() {
  return worker_schedulers_;
}

}  // namespace scheduler
}  // namespace blink
