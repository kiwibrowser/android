// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/blame_context.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/child/features.h"
#include "third_party/blink/renderer/platform/scheduler/child/task_queue_with_task_type.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_visibility_state.h"
#include "third_party/blink/renderer/platform/scheduler/util/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"

namespace blink {

namespace scheduler {

using base::sequence_manager::TaskQueue;

namespace {

const char* VisibilityStateToString(bool is_visible) {
  if (is_visible) {
    return "visible";
  } else {
    return "hidden";
  }
}

const char* PausedStateToString(bool is_paused) {
  if (is_paused) {
    return "paused";
  } else {
    return "running";
  }
}

const char* FrozenStateToString(bool is_frozen) {
  if (is_frozen) {
    return "frozen";
  } else {
    return "running";
  }
}

const char* KeepActiveStateToString(bool keep_active) {
  if (keep_active) {
    return "keep_active";
  } else {
    return "no_keep_active";
  }
}

// Used to update the priority of task_queue. Note that this function is
// used for queues associated with a frame.
void UpdatePriority(MainThreadTaskQueue* task_queue) {
  if (!task_queue)
    return;

  FrameSchedulerImpl* frame_scheduler = task_queue->GetFrameScheduler();
  DCHECK(frame_scheduler);
  task_queue->SetQueuePriority(frame_scheduler->ComputePriority(task_queue));
}

}  // namespace

FrameSchedulerImpl::ActiveConnectionHandleImpl::ActiveConnectionHandleImpl(
    FrameSchedulerImpl* frame_scheduler)
    : frame_scheduler_(frame_scheduler->GetWeakPtr()) {
  frame_scheduler->DidOpenActiveConnection();
}

FrameSchedulerImpl::ActiveConnectionHandleImpl::~ActiveConnectionHandleImpl() {
  if (frame_scheduler_) {
    static_cast<FrameSchedulerImpl*>(frame_scheduler_.get())
        ->DidCloseActiveConnection();
  }
}

FrameSchedulerImpl::PauseSubresourceLoadingHandleImpl::
    PauseSubresourceLoadingHandleImpl(
        base::WeakPtr<FrameSchedulerImpl> frame_scheduler)
    : frame_scheduler_(std::move(frame_scheduler)) {
  DCHECK(frame_scheduler_);
  frame_scheduler_->AddPauseSubresourceLoadingHandle();
}

FrameSchedulerImpl::PauseSubresourceLoadingHandleImpl::
    ~PauseSubresourceLoadingHandleImpl() {
  if (frame_scheduler_)
    frame_scheduler_->RemovePauseSubresourceLoadingHandle();
}

std::unique_ptr<FrameSchedulerImpl> FrameSchedulerImpl::Create(
    PageSchedulerImpl* parent_page_scheduler,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type) {
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler(
      new FrameSchedulerImpl(parent_page_scheduler->GetMainThreadScheduler(),
                             parent_page_scheduler, blame_context, frame_type));
  parent_page_scheduler->RegisterFrameSchedulerImpl(frame_scheduler.get());
  return frame_scheduler;
}

FrameSchedulerImpl::FrameSchedulerImpl(
    MainThreadSchedulerImpl* main_thread_scheduler,
    PageSchedulerImpl* parent_page_scheduler,
    base::trace_event::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type)
    : frame_type_(frame_type),
      main_thread_scheduler_(main_thread_scheduler),
      parent_page_scheduler_(parent_page_scheduler),
      blame_context_(blame_context),
      throttling_state_(SchedulingLifecycleState::kNotThrottled),
      frame_visible_(true,
                     "FrameScheduler.FrameVisible",
                     this,
                     &tracing_controller_,
                     VisibilityStateToString),
      frame_paused_(false,
                    "FrameScheduler.FramePaused",
                    this,
                    &tracing_controller_,
                    PausedStateToString),
      frame_origin_type_(frame_type == FrameType::kMainFrame
                             ? FrameOriginType::kMainFrame
                             : FrameOriginType::kSameOriginFrame,
                         "FrameScheduler.Origin",
                         this,
                         &tracing_controller_,
                         FrameOriginTypeToString),
      subresource_loading_paused_(false,
                                  "FrameScheduler.SubResourceLoadingPaused",
                                  this,
                                  &tracing_controller_,
                                  PausedStateToString),
      url_tracer_("FrameScheduler.URL", this),
      task_queue_throttled_(false,
                            "FrameScheduler.TaskQueueThrottled",
                            this,
                            &tracing_controller_,
                            YesNoStateToString),
      active_connection_count_(0),
      subresource_loading_pause_count_(0u),
      has_active_connection_(false,
                             "FrameScheduler.HasActiveConnection",
                             this,
                             &tracing_controller_,
                             YesNoStateToString),
      page_frozen_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->IsFrozen() : true,
          "FrameScheduler.PageFrozen",
          this,
          &tracing_controller_,
          FrozenStateToString),
      page_visibility_for_tracing_(
          parent_page_scheduler_ && parent_page_scheduler_->IsPageVisible()
              ? PageVisibilityState::kVisible
              : PageVisibilityState::kHidden,
          "FrameScheduler.PageVisibility",
          this,
          &tracing_controller_,
          PageVisibilityStateToString),
      page_keep_active_for_tracing_(
          parent_page_scheduler_ ? parent_page_scheduler_->KeepActive() : false,
          "FrameScheduler.KeepActive",
          this,
          &tracing_controller_,
          KeepActiveStateToString),
      weak_factory_(this) {}

FrameSchedulerImpl::FrameSchedulerImpl()
    : FrameSchedulerImpl(nullptr, nullptr, nullptr, FrameType::kSubframe) {}

namespace {

void CleanUpQueue(MainThreadTaskQueue* queue) {
  if (!queue)
    return;
  queue->DetachFromMainThreadScheduler();
  queue->DetachFromFrameScheduler();
  queue->SetBlameContext(nullptr);
  queue->SetQueuePriority(TaskQueue::QueuePriority::kLowPriority);
}

}  // namespace

FrameSchedulerImpl::~FrameSchedulerImpl() {
  weak_factory_.InvalidateWeakPtrs();

  RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();

  CleanUpQueue(loading_task_queue_.get());
  CleanUpQueue(loading_control_task_queue_.get());
  CleanUpQueue(throttleable_task_queue_.get());
  CleanUpQueue(deferrable_task_queue_.get());
  CleanUpQueue(pausable_task_queue_.get());
  CleanUpQueue(unpausable_task_queue_.get());

  if (parent_page_scheduler_) {
    parent_page_scheduler_->Unregister(this);

    if (has_active_connection())
      parent_page_scheduler_->OnConnectionUpdated();
  }
}

void FrameSchedulerImpl::DetachFromPageScheduler() {
  RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool();

  parent_page_scheduler_ = nullptr;
}

void FrameSchedulerImpl::
    RemoveThrottleableQueueFromBackgroundCPUTimeBudgetPool() {
  if (!throttleable_task_queue_)
    return;

  if (!parent_page_scheduler_)
    return;

  CPUTimeBudgetPool* time_budget_pool =
      parent_page_scheduler_->BackgroundCPUTimeBudgetPool();

  if (!time_budget_pool)
    return;

  time_budget_pool->RemoveQueue(
      main_thread_scheduler_->tick_clock()->NowTicks(),
      throttleable_task_queue_.get());
}

void FrameSchedulerImpl::SetFrameVisible(bool frame_visible) {
  DCHECK(parent_page_scheduler_);
  if (frame_visible_ == frame_visible)
    return;
  UMA_HISTOGRAM_BOOLEAN("RendererScheduler.IPC.FrameVisibility", frame_visible);
  frame_visible_ = frame_visible;
  UpdatePolicy();
  UpdateQueuePriorities();
}

bool FrameSchedulerImpl::IsFrameVisible() const {
  return frame_visible_;
}

void FrameSchedulerImpl::SetCrossOrigin(bool cross_origin) {
  DCHECK(parent_page_scheduler_);
  if (frame_origin_type_ == FrameOriginType::kMainFrame) {
    DCHECK(!cross_origin);
    return;
  }
  if (cross_origin) {
    frame_origin_type_ = FrameOriginType::kCrossOriginFrame;
  } else {
    frame_origin_type_ = FrameOriginType::kSameOriginFrame;
  }
  UpdatePolicy();
}

bool FrameSchedulerImpl::IsCrossOrigin() const {
  return frame_origin_type_ == FrameOriginType::kCrossOriginFrame;
}

void FrameSchedulerImpl::TraceUrlChange(const String& url) {
  url_tracer_.TraceString(url);
}

FrameScheduler::FrameType FrameSchedulerImpl::GetFrameType() const {
  return frame_type_;
}

scoped_refptr<base::SingleThreadTaskRunner> FrameSchedulerImpl::GetTaskRunner(
    TaskType type) {
  // TODO(haraken): Optimize the mapping from TaskTypes to task runners.
  switch (type) {
    case TaskType::kJavascriptTimer:
      return TaskQueueWithTaskType::Create(ThrottleableTaskQueue(), type);
    case TaskType::kInternalLoading:
    case TaskType::kNetworking:
      return TaskQueueWithTaskType::Create(LoadingTaskQueue(), type);
    case TaskType::kNetworkingControl:
      return TaskQueueWithTaskType::Create(LoadingControlTaskQueue(), type);
    // Throttling following tasks may break existing web pages, so tentatively
    // these are unthrottled.
    // TODO(nhiroki): Throttle them again after we're convinced that it's safe
    // or provide a mechanism that web pages can opt-out it if throttling is not
    // desirable.
    case TaskType::kDatabaseAccess:
    case TaskType::kDOMManipulation:
    case TaskType::kHistoryTraversal:
    case TaskType::kEmbed:
    case TaskType::kCanvasBlobSerialization:
    case TaskType::kRemoteEvent:
    case TaskType::kWebSocket:
    case TaskType::kMicrotask:
    case TaskType::kUnshippedPortMessage:
    case TaskType::kFileReading:
    case TaskType::kPresentation:
    case TaskType::kSensor:
    case TaskType::kPerformanceTimeline:
    case TaskType::kWebGL:
    case TaskType::kIdleTask:
    case TaskType::kInternalDefault:
    case TaskType::kMiscPlatformAPI:
      // TODO(altimin): Move appropriate tasks to throttleable task queue.
      return TaskQueueWithTaskType::Create(DeferrableTaskQueue(), type);
    // PostedMessage can be used for navigation, so we shouldn't defer it
    // when expecting a user gesture.
    case TaskType::kPostedMessage:
    // UserInteraction tasks should be run even when expecting a user gesture.
    case TaskType::kUserInteraction:
    // Media events should not be deferred to ensure that media playback is
    // smooth.
    case TaskType::kMediaElementEvent:
    case TaskType::kInternalTest:
    case TaskType::kInternalWebCrypto:
    case TaskType::kInternalIndexedDB:
    case TaskType::kInternalMedia:
    case TaskType::kInternalMediaRealTime:
    case TaskType::kInternalUserInteraction:
    case TaskType::kInternalIntersectionObserver:
      return TaskQueueWithTaskType::Create(PausableTaskQueue(), type);
    case TaskType::kInternalIPC:
    // The TaskType of Inspector tasks needs to be unpausable because they need
    // to run even on a paused page.
    case TaskType::kInternalInspector:
    // The TaskType of worker tasks needs to be unpausable (in addition to
    // unthrottled and undeferred) not to prevent service workers that may
    // control browser navigation on multiple tabs.
    case TaskType::kInternalWorker:
      return TaskQueueWithTaskType::Create(UnpausableTaskQueue(), type);
    case TaskType::kDeprecatedNone:
    case TaskType::kMainThreadTaskQueueV8:
    case TaskType::kMainThreadTaskQueueCompositor:
    case TaskType::kMainThreadTaskQueueDefault:
    case TaskType::kMainThreadTaskQueueInput:
    case TaskType::kMainThreadTaskQueueIdle:
    case TaskType::kMainThreadTaskQueueIPC:
    case TaskType::kMainThreadTaskQueueControl:
    case TaskType::kCompositorThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueDefault:
    case TaskType::kWorkerThreadTaskQueueV8:
    case TaskType::kWorkerThreadTaskQueueCompositor:
    case TaskType::kCount:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return nullptr;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::LoadingTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!loading_task_queue_) {
    // TODO(panicker): Avoid adding this queue in RS task_runners_.
    loading_task_queue_ = main_thread_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameLoading, this);
    loading_task_queue_->SetBlameContext(blame_context_);
    loading_queue_enabled_voter_ =
        loading_task_queue_->CreateQueueEnabledVoter();
    loading_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return loading_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::LoadingControlTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!loading_control_task_queue_) {
    loading_control_task_queue_ = main_thread_scheduler_->NewLoadingTaskQueue(
        MainThreadTaskQueue::QueueType::kFrameLoadingControl, this);
    loading_control_task_queue_->SetBlameContext(blame_context_);
    loading_control_queue_enabled_voter_ =
        loading_control_task_queue_->CreateQueueEnabledVoter();
    loading_control_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return loading_control_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::ThrottleableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!throttleable_task_queue_) {
    // TODO(panicker): Avoid adding this queue in RS task_runners_.
    throttleable_task_queue_ = main_thread_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetCanBeThrottled(true)
            .SetCanBeFrozen(true)
            .SetFreezeWhenKeepActive(true)
            .SetCanBeDeferred(true)
            .SetCanBePaused(true)
            .SetFrameScheduler(this));
    throttleable_task_queue_->SetBlameContext(blame_context_);
    throttleable_queue_enabled_voter_ =
        throttleable_task_queue_->CreateQueueEnabledVoter();
    throttleable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);

    CPUTimeBudgetPool* time_budget_pool =
        parent_page_scheduler_->BackgroundCPUTimeBudgetPool();
    if (time_budget_pool) {
      time_budget_pool->AddQueue(
          main_thread_scheduler_->tick_clock()->NowTicks(),
          throttleable_task_queue_.get());
    }
    UpdateThrottling();
  }
  return throttleable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::DeferrableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!deferrable_task_queue_) {
    deferrable_task_queue_ = main_thread_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameDeferrable)
            .SetCanBeDeferred(true)
            .SetCanBeFrozen(
                RuntimeEnabledFeatures::StopNonTimersInBackgroundEnabled())
            .SetCanBePaused(true)
            .SetFrameScheduler(this));
    deferrable_task_queue_->SetBlameContext(blame_context_);
    deferrable_queue_enabled_voter_ =
        deferrable_task_queue_->CreateQueueEnabledVoter();
    deferrable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return deferrable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::PausableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!pausable_task_queue_) {
    pausable_task_queue_ = main_thread_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFramePausable)
            .SetCanBeFrozen(
                RuntimeEnabledFeatures::StopNonTimersInBackgroundEnabled())
            .SetCanBePaused(true)
            .SetFrameScheduler(this));
    pausable_task_queue_->SetBlameContext(blame_context_);
    pausable_queue_enabled_voter_ =
        pausable_task_queue_->CreateQueueEnabledVoter();
    pausable_queue_enabled_voter_->SetQueueEnabled(!frame_paused_);
  }
  return pausable_task_queue_;
}

scoped_refptr<TaskQueue> FrameSchedulerImpl::UnpausableTaskQueue() {
  DCHECK(parent_page_scheduler_);
  if (!unpausable_task_queue_) {
    unpausable_task_queue_ = main_thread_scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameUnpausable)
            .SetFrameScheduler(this));
    unpausable_task_queue_->SetBlameContext(blame_context_);
  }
  return unpausable_task_queue_;
}

scoped_refptr<base::SingleThreadTaskRunner>
FrameSchedulerImpl::ControlTaskRunner() {
  DCHECK(parent_page_scheduler_);
  return main_thread_scheduler_->ControlTaskRunner();
}

blink::PageScheduler* FrameSchedulerImpl::GetPageScheduler() const {
  return parent_page_scheduler_;
}

void FrameSchedulerImpl::DidStartProvisionalLoad(bool is_main_frame) {
  main_thread_scheduler_->DidStartProvisionalLoad(is_main_frame);
}

void FrameSchedulerImpl::DidCommitProvisionalLoad(
    bool is_web_history_inert_commit,
    bool is_reload,
    bool is_main_frame) {
  main_thread_scheduler_->DidCommitProvisionalLoad(is_web_history_inert_commit,
                                                   is_reload, is_main_frame);
}

WebScopedVirtualTimePauser FrameSchedulerImpl::CreateWebScopedVirtualTimePauser(
    const WTF::String& name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(main_thread_scheduler_, duration, name);
}

void FrameSchedulerImpl::DidOpenActiveConnection() {
  ++active_connection_count_;
  has_active_connection_ = static_cast<bool>(active_connection_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnConnectionUpdated();
}

void FrameSchedulerImpl::DidCloseActiveConnection() {
  DCHECK_GT(active_connection_count_, 0);
  --active_connection_count_;
  has_active_connection_ = static_cast<bool>(active_connection_count_);
  if (parent_page_scheduler_)
    parent_page_scheduler_->OnConnectionUpdated();
}

void FrameSchedulerImpl::UpdateQueuePriorities() {
  UpdatePriority(loading_task_queue_.get());
  UpdatePriority(loading_control_task_queue_.get());
  UpdatePriority(throttleable_task_queue_.get());
  UpdatePriority(deferrable_task_queue_.get());
  UpdatePriority(pausable_task_queue_.get());
  UpdatePriority(unpausable_task_queue_.get());
}

void FrameSchedulerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetBoolean("frame_visible", frame_visible_);
  state->SetBoolean("page_visible", parent_page_scheduler_->IsPageVisible());
  state->SetBoolean("cross_origin", IsCrossOrigin());
  state->SetString("frame_type",
                   frame_type_ == FrameScheduler::FrameType::kMainFrame
                       ? "MainFrame"
                       : "Subframe");
  state->SetBoolean(
      "disable_background_timer_throttling",
      !RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled());
  if (loading_task_queue_) {
    state->SetString("loading_task_queue",
                     PointerToString(loading_task_queue_.get()));
  }
  if (loading_control_task_queue_) {
    state->SetString("loading_control_task_queue",
                     PointerToString(loading_control_task_queue_.get()));
  }
  if (throttleable_task_queue_) {
    state->SetString("throttleable_task_queue",
                     PointerToString(throttleable_task_queue_.get()));
  }
  if (deferrable_task_queue_) {
    state->SetString("deferrable_task_queue",
                     PointerToString(deferrable_task_queue_.get()));
  }
  if (pausable_task_queue_) {
    state->SetString("pausable_task_queue",
                     PointerToString(pausable_task_queue_.get()));
  }
  if (unpausable_task_queue_) {
    state->SetString("unpausable_task_queue",
                     PointerToString(unpausable_task_queue_.get()));
  }
  if (blame_context_) {
    state->BeginDictionary("blame_context");
    state->SetString(
        "id_ref",
        PointerToString(reinterpret_cast<void*>(blame_context_->id())));
    state->SetString("scope", blame_context_->scope());
    state->EndDictionary();
  }
}

void FrameSchedulerImpl::SetPageVisibilityForTracing(
    PageVisibilityState page_visibility) {
  page_visibility_for_tracing_ = page_visibility;
}

bool FrameSchedulerImpl::IsPageVisible() const {
  return parent_page_scheduler_ ? parent_page_scheduler_->IsPageVisible()
                                : true;
}

bool FrameSchedulerImpl::IsAudioPlaying() const {
  return parent_page_scheduler_ ? parent_page_scheduler_->IsAudioPlaying()
                                : false;
}

void FrameSchedulerImpl::SetPaused(bool frame_paused) {
  DCHECK(parent_page_scheduler_);
  if (frame_paused_ == frame_paused)
    return;

  frame_paused_ = frame_paused;
  UpdatePolicy();
}

void FrameSchedulerImpl::SetPageFrozenForTracing(bool frozen) {
  page_frozen_for_tracing_ = frozen;
}

void FrameSchedulerImpl::SetPageKeepActiveForTracing(bool keep_active) {
  page_keep_active_for_tracing_ = keep_active;
}

void FrameSchedulerImpl::UpdatePolicy() {
  // Per-frame (stoppable) task queues will be frozen after 5mins in
  // background. They will be resumed when the page is visible.
  UpdateQueuePolicy(throttleable_task_queue_,
                    throttleable_queue_enabled_voter_.get());
  UpdateQueuePolicy(loading_task_queue_, loading_queue_enabled_voter_.get());
  UpdateQueuePolicy(loading_control_task_queue_,
                    loading_control_queue_enabled_voter_.get());
  UpdateQueuePolicy(deferrable_task_queue_,
                    deferrable_queue_enabled_voter_.get());
  UpdateQueuePolicy(pausable_task_queue_, pausable_queue_enabled_voter_.get());

  UpdateThrottling();

  NotifyLifecycleObservers();
}

void FrameSchedulerImpl::UpdateQueuePolicy(
    const scoped_refptr<MainThreadTaskQueue>& queue,
    TaskQueue::QueueEnabledVoter* voter) {
  if (!queue || !voter)
    return;
  DCHECK(parent_page_scheduler_);
  bool queue_paused = frame_paused_ && queue->CanBePaused();
  bool queue_frozen =
      parent_page_scheduler_->IsFrozen() && queue->CanBeFrozen();
  // Override freezing if keep-active is true.
  if (queue_frozen && !queue->FreezeWhenKeepActive())
    queue_frozen = !parent_page_scheduler_->KeepActive();
  voter->SetQueueEnabled(!queue_paused && !queue_frozen);
}

SchedulingLifecycleState FrameSchedulerImpl::CalculateLifecycleState(
    ObserverType type) const {
  // Detached frames are not throttled.
  if (!parent_page_scheduler_)
    return SchedulingLifecycleState::kNotThrottled;

  if (RuntimeEnabledFeatures::StopLoadingInBackgroundEnabled() &&
      parent_page_scheduler_->IsFrozen() &&
      !parent_page_scheduler_->KeepActive()) {
    DCHECK(!parent_page_scheduler_->IsPageVisible());
    return SchedulingLifecycleState::kStopped;
  }
  if (subresource_loading_paused_ && type == ObserverType::kLoader)
    return SchedulingLifecycleState::kStopped;
  if (type == ObserverType::kLoader &&
      parent_page_scheduler_->HasActiveConnection()) {
    return SchedulingLifecycleState::kNotThrottled;
  }
  if (parent_page_scheduler_->IsThrottled())
    return SchedulingLifecycleState::kThrottled;
  if (!parent_page_scheduler_->IsPageVisible())
    return SchedulingLifecycleState::kHidden;
  return SchedulingLifecycleState::kNotThrottled;
}

void FrameSchedulerImpl::OnFirstMeaningfulPaint() {
  main_thread_scheduler_->OnFirstMeaningfulPaint();
}

std::unique_ptr<FrameScheduler::ActiveConnectionHandle>
FrameSchedulerImpl::OnActiveConnectionCreated() {
  return std::make_unique<FrameSchedulerImpl::ActiveConnectionHandleImpl>(this);
}

bool FrameSchedulerImpl::ShouldThrottleTimers() const {
  if (!RuntimeEnabledFeatures::TimerThrottlingForBackgroundTabsEnabled())
    return false;
  if (parent_page_scheduler_ && parent_page_scheduler_->IsAudioPlaying())
    return false;
  if (!parent_page_scheduler_->IsPageVisible())
    return true;
  return RuntimeEnabledFeatures::TimerThrottlingForHiddenFramesEnabled() &&
         !frame_visible_ && IsCrossOrigin();
}

void FrameSchedulerImpl::UpdateThrottling() {
  // Before we initialize a trottleable task queue, |task_queue_throttled_|
  // stays false and this function ensures it indicates whether are we holding
  // a queue reference for throttler or not.
  // Don't modify that value neither amend the reference counter anywhere else.
  if (!throttleable_task_queue_)
    return;
  bool should_throttle = ShouldThrottleTimers();
  if (task_queue_throttled_ == should_throttle)
    return;
  task_queue_throttled_ = should_throttle;

  if (should_throttle) {
    main_thread_scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
        throttleable_task_queue_.get());
  } else {
    main_thread_scheduler_->task_queue_throttler()->DecreaseThrottleRefCount(
        throttleable_task_queue_.get());
  }
}

bool FrameSchedulerImpl::IsExemptFromBudgetBasedThrottling() const {
  return has_active_connection();
}

TaskQueue::QueuePriority FrameSchedulerImpl::ComputePriority(
    MainThreadTaskQueue* task_queue) const {
  DCHECK(task_queue);

  FrameScheduler* frame_scheduler = task_queue->GetFrameScheduler();

  // Checks the task queue is associated with this frame scheduler.
  DCHECK_EQ(frame_scheduler, this);

  base::Optional<TaskQueue::QueuePriority> fixed_priority =
      task_queue->FixedPriority();

  if (fixed_priority)
    return fixed_priority.value();

  bool background_page_with_no_audio = !IsPageVisible() && !IsAudioPlaying();

  if (background_page_with_no_audio) {
    if (base::FeatureList::IsEnabled(kLowPriorityForBackgroundPages))
      return TaskQueue::QueuePriority::kLowPriority;

    if (base::FeatureList::IsEnabled(kBestEffortPriorityForBackgroundPages))
      return TaskQueue::QueuePriority::kBestEffortPriority;
  }

  // If main thread is in the loading use case or if the priority experiments
  // should take place at all times.
  if (main_thread_scheduler_->IsLoading() ||
      !base::FeatureList::IsEnabled(kExperimentOnlyWhenLoading)) {
    // Low priority feature enabled for hidden frame.
    if (base::FeatureList::IsEnabled(kLowPriorityForHiddenFrame) &&
        !IsFrameVisible())
      return TaskQueue::QueuePriority::kLowPriority;

    bool is_subframe = GetFrameType() == FrameScheduler::FrameType::kSubframe;
    bool is_throttleable_task_queue =
        task_queue->queue_type() ==
        MainThreadTaskQueue::QueueType::kFrameThrottleable;

    // Low priority feature enabled for sub-frame.
    if (base::FeatureList::IsEnabled(kLowPriorityForSubFrame) && is_subframe)
      return TaskQueue::QueuePriority::kLowPriority;

    // Low priority feature enabled for sub-frame throttleable task queues.
    if (base::FeatureList::IsEnabled(kLowPriorityForSubFrameThrottleableTask) &&
        is_throttleable_task_queue)
      return TaskQueue::QueuePriority::kLowPriority;

    // Low priority feature enabled for throttleable task queues.
    if (base::FeatureList::IsEnabled(kLowPriorityForThrottleableTask) &&
        is_throttleable_task_queue)
      return TaskQueue::QueuePriority::kLowPriority;
  }

  // TODO(farahcharab) Change highest priority to high priority for frame
  // loading control.
  return task_queue->queue_type() ==
                 MainThreadTaskQueue::QueueType::kFrameLoadingControl
             ? TaskQueue::QueuePriority::kHighestPriority
             : TaskQueue::QueuePriority::kNormalPriority;
}

std::unique_ptr<blink::mojom::blink::PauseSubresourceLoadingHandle>
FrameSchedulerImpl::GetPauseSubresourceLoadingHandle() {
  return std::make_unique<PauseSubresourceLoadingHandleImpl>(
      weak_factory_.GetWeakPtr());
}

void FrameSchedulerImpl::AddPauseSubresourceLoadingHandle() {
  ++subresource_loading_pause_count_;
  if (subresource_loading_pause_count_ != 1) {
    DCHECK(subresource_loading_paused_);
    return;
  }

  DCHECK(!subresource_loading_paused_);
  subresource_loading_paused_ = true;
  UpdatePolicy();
}

void FrameSchedulerImpl::RemovePauseSubresourceLoadingHandle() {
  DCHECK_LT(0u, subresource_loading_pause_count_);
  --subresource_loading_pause_count_;
  DCHECK(subresource_loading_paused_);
  if (subresource_loading_pause_count_ == 0) {
    subresource_loading_paused_ = false;
    UpdatePolicy();
  }
}

}  // namespace scheduler
}  // namespace blink
