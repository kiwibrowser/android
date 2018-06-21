// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/task_queue_manager_for_test.h"
#include "third_party/blink/renderer/platform/scheduler/child/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

using base::sequence_manager::TaskQueue;
using testing::UnorderedElementsAre;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace frame_scheduler_impl_unittest {

class FrameSchedulerImplTest : public testing::Test {
 public:
  FrameSchedulerImplTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {
    // Null clock might trigger some assertions.
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5));
  }

  ~FrameSchedulerImplTest() override = default;

  void SetUp() override {
    scheduler_.reset(new MainThreadSchedulerImpl(
        base::sequence_manager::TaskQueueManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock()),
        base::nullopt));
    page_scheduler_.reset(new PageSchedulerImpl(nullptr, scheduler_.get()));
    frame_scheduler_ = FrameSchedulerImpl::Create(
        page_scheduler_.get(), nullptr, FrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

 protected:
  scoped_refptr<TaskQueue> throttleable_task_queue() {
    return frame_scheduler_->throttleable_task_queue_;
  }

  void LazyInitThrottleableTaskQueue() {
    EXPECT_FALSE(throttleable_task_queue());
    frame_scheduler_->ThrottleableTaskQueue();
    EXPECT_TRUE(throttleable_task_queue());
  }

  scoped_refptr<TaskQueue> ThrottleableTaskQueue() {
    return frame_scheduler_->ThrottleableTaskQueue();
  }

  scoped_refptr<TaskQueue> LoadingTaskQueue() {
    return frame_scheduler_->LoadingTaskQueue();
  }

  scoped_refptr<TaskQueue> LoadingControlTaskQueue() {
    return frame_scheduler_->LoadingControlTaskQueue();
  }

  scoped_refptr<TaskQueue> DeferrableTaskQueue() {
    return frame_scheduler_->DeferrableTaskQueue();
  }

  scoped_refptr<TaskQueue> PausableTaskQueue() {
    return frame_scheduler_->PausableTaskQueue();
  }

  scoped_refptr<TaskQueue> UnpausableTaskQueue() {
    return frame_scheduler_->UnpausableTaskQueue();
  }

  bool IsThrottled() {
    EXPECT_TRUE(throttleable_task_queue());
    return scheduler_->task_queue_throttler()->IsThrottled(
        throttleable_task_queue().get());
  }

  SchedulingLifecycleState CalculateLifecycleState(
      FrameScheduler::ObserverType type) {
    return frame_scheduler_->CalculateLifecycleState(type);
  }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
};

namespace {

class MockLifecycleObserver final : public FrameScheduler::Observer {
 public:
  MockLifecycleObserver()
      : not_throttled_count_(0u),
        hidden_count_(0u),
        throttled_count_(0u),
        stopped_count_(0u) {}

  inline void CheckObserverState(base::Location from,
                                 size_t not_throttled_count_expectation,
                                 size_t hidden_count_expectation,
                                 size_t throttled_count_expectation,
                                 size_t stopped_count_expectation) {
    EXPECT_EQ(not_throttled_count_expectation, not_throttled_count_)
        << from.ToString();
    EXPECT_EQ(hidden_count_expectation, hidden_count_) << from.ToString();
    EXPECT_EQ(throttled_count_expectation, throttled_count_) << from.ToString();
    EXPECT_EQ(stopped_count_expectation, stopped_count_) << from.ToString();
  }

  void OnLifecycleStateChanged(SchedulingLifecycleState state) override {
    switch (state) {
      case SchedulingLifecycleState::kNotThrottled:
        not_throttled_count_++;
        break;
      case SchedulingLifecycleState::kHidden:
        hidden_count_++;
        break;
      case SchedulingLifecycleState::kThrottled:
        throttled_count_++;
        break;
      case SchedulingLifecycleState::kStopped:
        stopped_count_++;
        break;
        // We should not have another state, and compiler checks it.
    }
  }

 private:
  size_t not_throttled_count_;
  size_t hidden_count_;
  size_t throttled_count_;
  size_t stopped_count_;
};

void IncrementCounter(int* counter) {
  ++*counter;
}

void RecordQueueName(const scoped_refptr<TaskQueue> task_queue,
                     std::vector<std::string>* tasks) {
  tasks->push_back(task_queue->GetName());
}

}  // namespace

// Throttleable task queue is initialized lazily, so there're two scenarios:
// - Task queue created first and throttling decision made later;
// - Scheduler receives relevant signals to make a throttling decision but
//   applies one once task queue gets created.
// We test both (ExplicitInit/LazyInit) of them.

TEST_F(FrameSchedulerImplTest, PageVisible) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  EXPECT_FALSE(throttleable_task_queue());
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  page_scheduler_->SetPageVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHiddenThenVisible_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHiddenThenVisible_CrossOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  frame_scheduler_->SetCrossOrigin(false);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_TRUE(IsThrottled());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHidden_CrossOrigin_NoThrottling_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_NoThrottling_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  EXPECT_TRUE(throttleable_task_queue());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(true);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PauseAndResume) {
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetPaused(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  frame_scheduler_->SetPaused(false);

  EXPECT_EQ(1, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndUnfreezeFlagEnabled) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(true);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(true);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  // unpausable tasks continue to run.
  EXPECT_EQ(1, counter);

  page_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(1, counter);
  // Same as RunUntilIdle but also advances the cock if necessary.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndUnfreezeFlagDisabled) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(false);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(false);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  // throttleable tasks are frozen, other tasks continue to run.
  EXPECT_EQ(4, counter);

  page_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(4, counter);
  // Same as RunUntilIdle but also advances the clock if necessary.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeWithKeepActive) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(true);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(false);
  std::vector<std::string> tasks;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, LoadingTaskQueue(), &tasks));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, ThrottleableTaskQueue(), &tasks));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, DeferrableTaskQueue(), &tasks));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, PausableTaskQueue(), &tasks));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, UnpausableTaskQueue(), &tasks));

  page_scheduler_->SetKeepActive(true);  // say we have a Service Worker
  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // Everything runs except throttleable tasks (timers)
  EXPECT_THAT(tasks, UnorderedElementsAre(
                         std::string(LoadingTaskQueue()->GetName()),
                         std::string(DeferrableTaskQueue()->GetName()),
                         std::string(PausableTaskQueue()->GetName()),
                         std::string(UnpausableTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, LoadingTaskQueue(), &tasks));

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // loading task runs
  EXPECT_THAT(tasks,
              UnorderedElementsAre(std::string(LoadingTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, LoadingTaskQueue(), &tasks));
  // KeepActive is false when Service Worker stops.
  page_scheduler_->SetKeepActive(false);
  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(tasks, UnorderedElementsAre());  // loading task does not run

  tasks.clear();
  page_scheduler_->SetKeepActive(true);
  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // loading task runs
  EXPECT_THAT(tasks,
              UnorderedElementsAre(std::string(LoadingTaskQueue()->GetName())));
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndPageVisible) {
  ScopedStopLoadingInBackgroundForTest stop_loading_enabler(true);
  ScopedStopNonTimersInBackgroundForTest stop_non_timers_enabler(true);
  int counter = 0;
  LoadingTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  // Making the page visible should cause frozen queues to resume.
  page_scheduler_->SetPageVisible(true);

  EXPECT_EQ(1, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, counter);
}

// Tests if throttling observer interfaces work.
TEST_F(FrameSchedulerImplTest, LifecycleObserver) {
  std::unique_ptr<MockLifecycleObserver> observer =
      std::make_unique<MockLifecycleObserver>();

  size_t not_throttled_count = 0u;
  size_t hidden_count = 0u;
  size_t throttled_count = 0u;
  size_t stopped_count = 0u;

  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  auto observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kLoader, observer.get());

  // Initial state should be synchronously notified here.
  // We assume kNotThrottled is notified as an initial state, but it could
  // depend on implementation details and can be changed.
  observer->CheckObserverState(FROM_HERE, ++not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  // Once the page gets to be invisible, it should notify the observer of
  // kHidden synchronously.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, ++hidden_count,
                               throttled_count, stopped_count);

  // We do not issue new notifications without actually changing visibility
  // state.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));

  // The frame gets throttled after some time in background.
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               ++throttled_count, stopped_count);

  // We shouldn't issue new notifications for kThrottled state as well.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  // Setting background page to STOPPED, notifies observers of kStopped.
  page_scheduler_->SetPageFrozen(true);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, ++stopped_count);

  // When page is not in the STOPPED state, then page visibility is used,
  // notifying observer of kThrottled.
  page_scheduler_->SetPageFrozen(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               ++throttled_count, stopped_count);

  // Going back to visible state should notify the observer of kNotThrottled
  // synchronously.
  page_scheduler_->SetPageVisible(true);
  observer->CheckObserverState(FROM_HERE, ++not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  // Remove from the observer list, and see if any other callback should not be
  // invoked when the condition is changed.
  observer_handle.reset();
  page_scheduler_->SetPageVisible(false);

  // Wait 100 secs virtually and run pending tasks just in case.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(100));
  base::RunLoop().RunUntilIdle();

  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);
}

TEST_F(FrameSchedulerImplTest, DefaultSchedulingLifecycleState) {
  EXPECT_EQ(CalculateLifecycleState(FrameScheduler::ObserverType::kLoader),
            SchedulingLifecycleState::kNotThrottled);
  EXPECT_EQ(
      CalculateLifecycleState(FrameScheduler::ObserverType::kWorkerScheduler),
      SchedulingLifecycleState::kNotThrottled);
}

TEST_F(FrameSchedulerImplTest, SubesourceLoadingPaused) {
  // A loader observer and related counts.
  std::unique_ptr<MockLifecycleObserver> loader_observer =
      std::make_unique<MockLifecycleObserver>();

  size_t loader_throttled_count = 0u;
  size_t loader_not_throttled_count = 0u;
  size_t loader_hidden_count = 0u;
  size_t loader_stopped_count = 0u;

  // A worker observer and related counts.
  std::unique_ptr<MockLifecycleObserver> worker_observer =
      std::make_unique<MockLifecycleObserver>();

  size_t worker_throttled_count = 0u;
  size_t worker_not_throttled_count = 0u;
  size_t worker_hidden_count = 0u;
  size_t worker_stopped_count = 0u;

  // Both observers should start with no responses.
  loader_observer->CheckObserverState(
      FROM_HERE, loader_not_throttled_count, loader_hidden_count,
      loader_throttled_count, loader_stopped_count);

  worker_observer->CheckObserverState(
      FROM_HERE, worker_not_throttled_count, worker_hidden_count,
      worker_throttled_count, worker_stopped_count);

  // Adding the observers should recieve a non-throttled response
  auto loader_observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kLoader, loader_observer.get());

  auto worker_observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kWorkerScheduler, worker_observer.get());

  loader_observer->CheckObserverState(
      FROM_HERE, ++loader_not_throttled_count, loader_hidden_count,
      loader_throttled_count, loader_stopped_count);

  worker_observer->CheckObserverState(
      FROM_HERE, ++worker_not_throttled_count, worker_hidden_count,
      worker_throttled_count, worker_stopped_count);

  {
    auto pause_handle_a = frame_scheduler_->GetPauseSubresourceLoadingHandle();

    loader_observer->CheckObserverState(
        FROM_HERE, loader_not_throttled_count, loader_hidden_count,
        loader_throttled_count, ++loader_stopped_count);

    worker_observer->CheckObserverState(
        FROM_HERE, ++worker_not_throttled_count, worker_hidden_count,
        worker_throttled_count, worker_stopped_count);

    std::unique_ptr<MockLifecycleObserver> loader_observer_added_after_stopped =
        std::make_unique<MockLifecycleObserver>();

    auto loader_observer_handle = frame_scheduler_->AddLifecycleObserver(
        FrameScheduler::ObserverType::kLoader,
        loader_observer_added_after_stopped.get());
    // This observer should see stopped when added.
    loader_observer_added_after_stopped->CheckObserverState(FROM_HERE, 0, 0, 0,
                                                            1u);

    // Adding another handle should not create a new state.
    auto pause_handle_b = frame_scheduler_->GetPauseSubresourceLoadingHandle();

    loader_observer->CheckObserverState(
        FROM_HERE, loader_not_throttled_count, loader_hidden_count,
        loader_throttled_count, loader_stopped_count);

    worker_observer->CheckObserverState(
        FROM_HERE, worker_not_throttled_count, worker_hidden_count,
        worker_throttled_count, worker_stopped_count);
  }

  // Removing the handles should return the state to non throttled.
  loader_observer->CheckObserverState(
      FROM_HERE, ++loader_not_throttled_count, loader_hidden_count,
      loader_throttled_count, loader_stopped_count);

  worker_observer->CheckObserverState(
      FROM_HERE, ++worker_not_throttled_count, worker_hidden_count,
      worker_throttled_count, worker_stopped_count);
}

// TODO(farahcharab) Move priority testing to MainThreadTaskQueueTest after
// landing the change that moves priority computation to MainThreadTaskQueue.

TEST_F(FrameSchedulerImplTest, LowPriorityBackgroundPagesWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(kLowPriorityForBackgroundPages);

  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest,
       BestEffortPriorityBackgroundPagesWhenFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(kBestEffortPriorityForBackgroundPages);

  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest, LowPriorityForHiddenFrameFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(kLowPriorityForHiddenFrame);

  // Hidden Frame Task Queues.
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Visible Frame Task Queues.
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest,
       LowPriorityForHiddenFrameDuringLoadingFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      {kLowPriorityForHiddenFrame, kExperimentOnlyWhenLoading}, {});

  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(scheduler_->IsLoading());

  // Hidden Frame Task Queues.
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest, LowPriorityForSubFrameFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(kLowPriorityForSubFrame);

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
}

TEST_F(FrameSchedulerImplTest,
       LowPriorityForSubFrameDuringLoadingFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      {kLowPriorityForSubFrame, kExperimentOnlyWhenLoading}, {});

  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest,
       kLowPriorityForSubFrameThrottleableTaskFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(kLowPriorityForSubFrameThrottleableTask);

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest,
       kLowPriorityForSubFrameThrottleableTaskDuringLoadingFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      {kLowPriorityForSubFrameThrottleableTask, kExperimentOnlyWhenLoading},
      {});

  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest, kLowPriorityForThrottleableTaskFeatureEnabled) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndEnableFeature(kLowPriorityForThrottleableTask);

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_ = FrameSchedulerImpl::Create(
      page_scheduler_.get(), nullptr, FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest,
       kLowPriorityForThrottleableTaskDuringLoadingFeatureEnabled_SubFrame) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      {kLowPriorityForThrottleableTask, kExperimentOnlyWhenLoading}, {});

  // Main thread is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest,
       kLowPriorityForThrottleableTaskDuringLoadingFeatureEnabled_MainFrame) {
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      {kLowPriorityForThrottleableTask, kExperimentOnlyWhenLoading}, {});

  frame_scheduler_ = FrameSchedulerImpl::Create(
      page_scheduler_.get(), nullptr, FrameScheduler::FrameType::kMainFrame);

  // Main thread is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(scheduler_->IsLoading());

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(scheduler_->IsLoading());

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighestPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

}  // namespace frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
