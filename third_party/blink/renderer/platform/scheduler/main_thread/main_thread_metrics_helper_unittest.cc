// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_metrics_helper.h"

#include <memory>
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/renderer/platform/scheduler/base/test/task_queue_manager_for_test.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using base::sequence_manager::TaskQueue;

namespace blink {
namespace scheduler {

namespace {
class MainThreadSchedulerImplForTest : public MainThreadSchedulerImpl {
 public:
  MainThreadSchedulerImplForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager>
          task_queue_manager,
      base::Optional<base::Time> initial_virtual_time)
      : MainThreadSchedulerImpl(std::move(task_queue_manager),
                                initial_virtual_time){};

  using MainThreadSchedulerImpl::SetCurrentUseCaseForTest;
};
}  // namespace

using QueueType = MainThreadTaskQueue::QueueType;
using base::Bucket;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

class MainThreadMetricsHelperTest : public testing::Test {
 public:
  MainThreadMetricsHelperTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {
    // Null clock might trigger some assertions.
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));
  }

  ~MainThreadMetricsHelperTest() override = default;

  void SetUp() override {
    histogram_tester_.reset(new base::HistogramTester());
    scheduler_ = std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::TaskQueueManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock()),
        base::nullopt);
    metrics_helper_ = &scheduler_->main_thread_only().metrics_helper;
  }

  void TearDown() override {
    scheduler_->Shutdown();
    scheduler_.reset();
  }

  base::TimeTicks Now() {
    return task_environment_.GetMockTickClock()->NowTicks();
  }

  void FastForwardTo(base::TimeTicks time) {
    CHECK_LE(Now(), time);
    task_environment_.FastForwardBy(time - Now());
  }

  void RunTask(MainThreadTaskQueue::QueueType queue_type,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(Now(), start);
    FastForwardTo(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue;
    if (queue_type != MainThreadTaskQueue::QueueType::kDetached) {
      queue = scoped_refptr<MainThreadTaskQueueForTest>(
          new MainThreadTaskQueueForTest(queue_type));
    }

    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::OnceClosure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration, base::nullopt);
  }

  void RunTask(FrameSchedulerImpl* scheduler,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(Now(), start);
    FastForwardTo(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue(
        new MainThreadTaskQueueForTest(QueueType::kDefault));
    queue->SetFrameSchedulerForTest(scheduler);
    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::OnceClosure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration, base::nullopt);
  }

  void RunTask(UseCase use_case,
               base::TimeTicks start,
               base::TimeDelta duration) {
    DCHECK_LE(Now(), start);
    FastForwardTo(start + duration);
    scoped_refptr<MainThreadTaskQueueForTest> queue(
        new MainThreadTaskQueueForTest(QueueType::kDefault));
    scheduler_->SetCurrentUseCaseForTest(use_case);
    // Pass an empty task for recording.
    TaskQueue::PostedTask posted_task(base::OnceClosure(), FROM_HERE);
    TaskQueue::Task task(std::move(posted_task), base::TimeTicks());
    metrics_helper_->RecordTaskMetrics(queue.get(), task, start,
                                       start + duration, base::nullopt);
  }

  base::TimeTicks Milliseconds(int milliseconds) {
    return base::TimeTicks() + base::TimeDelta::FromMilliseconds(milliseconds);
  }

  base::TimeTicks Seconds(int seconds) {
    return base::TimeTicks() + base::TimeDelta::FromSeconds(seconds);
  }

  void ForceUpdatePolicy() { scheduler_->ForceUpdatePolicy(); }

  std::unique_ptr<FakeFrameScheduler> CreateFakeFrameSchedulerWithType(
      FrameStatus frame_status) {
    FakeFrameScheduler::Builder builder;
    switch (frame_status) {
      case FrameStatus::kNone:
      case FrameStatus::kDetached:
        return nullptr;
      case FrameStatus::kMainFrameVisible:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kMainFrameVisibleService:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetPageScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kMainFrameHidden:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kMainFrameHiddenService:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetPageScheduler(playing_view_.get());
        break;
      case FrameStatus::kMainFrameBackground:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame);
        break;
      case FrameStatus::kMainFrameBackgroundExemptSelf:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kMainFrameBackgroundExemptOther:
        builder.SetFrameType(FrameScheduler::FrameType::kMainFrame)
            .SetPageScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kSameOriginVisible:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kSameOriginVisibleService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetPageScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kSameOriginHidden:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kSameOriginHiddenService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetPageScheduler(playing_view_.get());
        break;
      case FrameStatus::kSameOriginBackground:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe);
        break;
      case FrameStatus::kSameOriginBackgroundExemptSelf:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kSameOriginBackgroundExemptOther:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetPageScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kCrossOriginVisible:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsPageVisible(true)
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kCrossOriginVisibleService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetPageScheduler(playing_view_.get())
            .SetIsFrameVisible(true);
        break;
      case FrameStatus::kCrossOriginHidden:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsPageVisible(true);
        break;
      case FrameStatus::kCrossOriginHiddenService:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetPageScheduler(playing_view_.get());
        break;
      case FrameStatus::kCrossOriginBackground:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true);
        break;
      case FrameStatus::kCrossOriginBackgroundExemptSelf:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetIsExemptFromThrottling(true);
        break;
      case FrameStatus::kCrossOriginBackgroundExemptOther:
        builder.SetFrameType(FrameScheduler::FrameType::kSubframe)
            .SetIsCrossOrigin(true)
            .SetPageScheduler(throtting_exempt_view_.get());
        break;
      case FrameStatus::kCount:
        NOTREACHED();
        return nullptr;
    }
    return builder.Build();
  }

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImplForTest> scheduler_;
  MainThreadMetricsHelper* metrics_helper_;  // NOT OWNED
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<FakePageScheduler> playing_view_ =
      FakePageScheduler::Builder().SetIsAudioPlaying(true).Build();
  std::unique_ptr<FakePageScheduler> throtting_exempt_view_ =
      FakePageScheduler::Builder().SetIsThrottlingExempt(true).Build();

  DISALLOW_COPY_AND_ASSIGN(MainThreadMetricsHelperTest);
};

TEST_F(MainThreadMetricsHelperTest, Metrics_PerQueueType) {
  // QueueType::kDefault is checking sub-millisecond task aggregation,
  // FRAME_* tasks are checking normal task aggregation and other
  // queue types have a single task.

  // Make sure that it starts in a foregrounded state.
  if (kLaunchingProcessIsBackgrounded)
    scheduler_->SetRendererBackgrounded(false);

  RunTask(QueueType::kDefault, Seconds(1),
          base::TimeDelta::FromMilliseconds(700));
  RunTask(QueueType::kDefault, Seconds(2),
          base::TimeDelta::FromMilliseconds(700));
  RunTask(QueueType::kDefault, Seconds(3),
          base::TimeDelta::FromMilliseconds(700));

  RunTask(QueueType::kControl, Seconds(4), base::TimeDelta::FromSeconds(3));
  RunTask(QueueType::kFrameLoading, Seconds(8),
          base::TimeDelta::FromSeconds(6));
  RunTask(QueueType::kFramePausable, Seconds(16),
          base::TimeDelta::FromSeconds(2));
  RunTask(QueueType::kCompositor, Seconds(19), base::TimeDelta::FromSeconds(2));
  RunTask(QueueType::kTest, Seconds(22), base::TimeDelta::FromSeconds(4));

  scheduler_->SetRendererBackgrounded(true);

  RunTask(QueueType::kControl, Seconds(26), base::TimeDelta::FromSeconds(2));
  RunTask(QueueType::kFrameThrottleable, Seconds(28),
          base::TimeDelta::FromSeconds(8));
  RunTask(QueueType::kUnthrottled, Seconds(38),
          base::TimeDelta::FromSeconds(5));
  RunTask(QueueType::kFrameLoading, Seconds(45),
          base::TimeDelta::FromSeconds(10));
  RunTask(QueueType::kFrameThrottleable, Seconds(60),
          base::TimeDelta::FromSeconds(5));
  RunTask(QueueType::kCompositor, Seconds(70),
          base::TimeDelta::FromSeconds(20));
  RunTask(QueueType::kIdle, Seconds(90), base::TimeDelta::FromSeconds(5));
  RunTask(QueueType::kFrameLoadingControl, Seconds(100),
          base::TimeDelta::FromSeconds(5));
  RunTask(QueueType::kControl, Seconds(106), base::TimeDelta::FromSeconds(6));
  RunTask(QueueType::kFrameThrottleable, Seconds(114),
          base::TimeDelta::FromSeconds(6));
  RunTask(QueueType::kFramePausable, Seconds(120),
          base::TimeDelta::FromSeconds(17));
  RunTask(QueueType::kIdle, Seconds(140), base::TimeDelta::FromSeconds(15));

  RunTask(QueueType::kDetached, Seconds(156), base::TimeDelta::FromSeconds(2));

  std::vector<base::Bucket> expected_samples = {
      {static_cast<int>(QueueType::kControl), 11},
      {static_cast<int>(QueueType::kDefault), 2},
      {static_cast<int>(QueueType::kUnthrottled), 5},
      {static_cast<int>(QueueType::kFrameLoading), 16},
      {static_cast<int>(QueueType::kCompositor), 22},
      {static_cast<int>(QueueType::kIdle), 20},
      {static_cast<int>(QueueType::kTest), 4},
      {static_cast<int>(QueueType::kFrameLoadingControl), 5},
      {static_cast<int>(QueueType::kFrameThrottleable), 19},
      {static_cast<int>(QueueType::kFramePausable), 19},
      {static_cast<int>(QueueType::kDetached), 2},
  };
  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType3"),
              testing::ContainerEq(expected_samples));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType3.Foreground"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(QueueType::kControl), 3),
                  Bucket(static_cast<int>(QueueType::kDefault), 2),
                  Bucket(static_cast<int>(QueueType::kFrameLoading), 6),
                  Bucket(static_cast<int>(QueueType::kCompositor), 2),
                  Bucket(static_cast<int>(QueueType::kTest), 4),
                  Bucket(static_cast<int>(QueueType::kFramePausable), 2)));

  EXPECT_THAT(histogram_tester_->GetAllSamples(
                  "RendererScheduler.TaskDurationPerQueueType3.Background"),
              UnorderedElementsAre(
                  Bucket(static_cast<int>(QueueType::kControl), 8),
                  Bucket(static_cast<int>(QueueType::kUnthrottled), 5),
                  Bucket(static_cast<int>(QueueType::kFrameLoading), 10),
                  Bucket(static_cast<int>(QueueType::kFrameThrottleable), 19),
                  Bucket(static_cast<int>(QueueType::kFramePausable), 17),
                  Bucket(static_cast<int>(QueueType::kCompositor), 20),
                  Bucket(static_cast<int>(QueueType::kIdle), 20),
                  Bucket(static_cast<int>(QueueType::kFrameLoadingControl), 5),
                  Bucket(static_cast<int>(QueueType::kDetached), 2)));
}

TEST_F(MainThreadMetricsHelperTest, Metrics_PerUseCase) {
  RunTask(UseCase::kNone, Milliseconds(500),
          base::TimeDelta::FromMilliseconds(400));

  RunTask(UseCase::kTouchstart, Seconds(1), base::TimeDelta::FromSeconds(2));
  RunTask(UseCase::kTouchstart, Seconds(3),
          base::TimeDelta::FromMilliseconds(300));
  RunTask(UseCase::kTouchstart, Seconds(4),
          base::TimeDelta::FromMilliseconds(300));

  RunTask(UseCase::kCompositorGesture, Seconds(5),
          base::TimeDelta::FromSeconds(5));
  RunTask(UseCase::kCompositorGesture, Seconds(10),
          base::TimeDelta::FromSeconds(3));

  RunTask(UseCase::kMainThreadCustomInputHandling, Seconds(14),
          base::TimeDelta::FromSeconds(2));
  RunTask(UseCase::kSynchronizedGesture, Seconds(17),
          base::TimeDelta::FromSeconds(2));
  RunTask(UseCase::kMainThreadCustomInputHandling, Seconds(19),
          base::TimeDelta::FromSeconds(5));
  RunTask(UseCase::kLoading, Seconds(25), base::TimeDelta::FromSeconds(6));
  RunTask(UseCase::kMainThreadGesture, Seconds(31),
          base::TimeDelta::FromSeconds(6));
  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskDurationPerUseCase2"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(UseCase::kTouchstart), 3),
          Bucket(static_cast<int>(UseCase::kCompositorGesture), 8),
          Bucket(static_cast<int>(UseCase::kMainThreadCustomInputHandling), 7),
          Bucket(static_cast<int>(UseCase::kSynchronizedGesture), 2),
          Bucket(static_cast<int>(UseCase::kLoading), 6),
          Bucket(static_cast<int>(UseCase::kMainThreadGesture), 6)));
}

TEST_F(MainThreadMetricsHelperTest, GetFrameStatusTest) {
  DCHECK_EQ(GetFrameStatus(nullptr), FrameStatus::kNone);

  FrameStatus frame_statuses_tested[] = {
      FrameStatus::kMainFrameVisible,
      FrameStatus::kSameOriginHidden,
      FrameStatus::kCrossOriginHidden,
      FrameStatus::kSameOriginBackground,
      FrameStatus::kMainFrameBackgroundExemptSelf,
      FrameStatus::kSameOriginVisibleService,
      FrameStatus::kCrossOriginHiddenService,
      FrameStatus::kMainFrameBackgroundExemptOther};
  for (FrameStatus frame_status : frame_statuses_tested) {
    std::unique_ptr<FakeFrameScheduler> frame =
        CreateFakeFrameSchedulerWithType(frame_status);
    EXPECT_EQ(GetFrameStatus(frame.get()), frame_status);
  }
}

TEST_F(MainThreadMetricsHelperTest, TaskCountPerFrameStatus) {
  int task_count = 0;
  struct CountPerFrameStatus {
    FrameStatus frame_status;
    int count;
  };
  CountPerFrameStatus test_data[] = {
      {FrameStatus::kNone, 4},
      {FrameStatus::kMainFrameVisible, 8},
      {FrameStatus::kMainFrameBackgroundExemptSelf, 5},
      {FrameStatus::kCrossOriginHidden, 3},
      {FrameStatus::kCrossOriginHiddenService, 7},
      {FrameStatus::kCrossOriginVisible, 1},
      {FrameStatus::kMainFrameBackgroundExemptOther, 2},
      {FrameStatus::kSameOriginVisible, 10},
      {FrameStatus::kSameOriginBackground, 9},
      {FrameStatus::kSameOriginVisibleService, 6}};

  for (const auto& data : test_data) {
    std::unique_ptr<FakeFrameScheduler> frame =
        CreateFakeFrameSchedulerWithType(data.frame_status);
    for (int i = 0; i < data.count; ++i) {
      RunTask(frame.get(), Milliseconds(++task_count),
              base::TimeDelta::FromMicroseconds(100));
    }
  }

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kNone), 4),
          Bucket(static_cast<int>(FrameStatus::kMainFrameVisible), 8),
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackgroundExemptSelf),
                 5),
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackgroundExemptOther),
                 2),
          Bucket(static_cast<int>(FrameStatus::kSameOriginVisible), 10),
          Bucket(static_cast<int>(FrameStatus::kSameOriginVisibleService), 6),
          Bucket(static_cast<int>(FrameStatus::kSameOriginBackground), 9),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisible), 1),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginHidden), 3),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginHiddenService), 7)));
}

TEST_F(MainThreadMetricsHelperTest, TaskCountPerFrameTypeLongerThan) {
  int total_duration = 0;
  struct TasksPerFrameStatus {
    FrameStatus frame_status;
    std::vector<int> durations;
  };
  TasksPerFrameStatus test_data[] = {
      {FrameStatus::kSameOriginHidden,
       {2, 15, 16, 20, 25, 30, 49, 50, 73, 99, 100, 110, 140, 150, 800, 1000,
        1200}},
      {FrameStatus::kCrossOriginVisibleService,
       {5, 10, 18, 19, 20, 55, 75, 220}},
      {FrameStatus::kMainFrameBackground,
       {21, 31, 41, 51, 61, 71, 81, 91, 101, 1001}},
  };

  for (const auto& data : test_data) {
    std::unique_ptr<FakeFrameScheduler> frame =
        CreateFakeFrameSchedulerWithType(data.frame_status);
    for (size_t i = 0; i < data.durations.size(); ++i) {
      RunTask(frame.get(), Milliseconds(++total_duration),
              base::TimeDelta::FromMilliseconds(data.durations[i]));
      total_duration += data.durations[i];
    }
  }

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 10),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 17),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 8)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType."
          "LongerThan16ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 10),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 15),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 6)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType."
          "LongerThan50ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 7),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 10),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 3)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType."
          "LongerThan100ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 2),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 7),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 1)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType."
          "LongerThan150ms"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 1),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 4),
          Bucket(static_cast<int>(FrameStatus::kCrossOriginVisibleService),
                 1)));

  EXPECT_THAT(
      histogram_tester_->GetAllSamples(
          "RendererScheduler.TaskCountPerFrameType.LongerThan1s"),
      UnorderedElementsAre(
          Bucket(static_cast<int>(FrameStatus::kMainFrameBackground), 1),
          Bucket(static_cast<int>(FrameStatus::kSameOriginHidden), 2)));
}

// TODO(crbug.com/754656): Add tests for NthMinute and
// AfterNthMinute histograms.

// TODO(crbug.com/754656): Add tests for
// TaskDuration.Hidden/Visible histograms.

// TODO(crbug.com/754656): Add tests for non-TaskDuration
// histograms.

}  // namespace scheduler
}  // namespace blink
