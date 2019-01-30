// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class ThreadStateSchedulingTest : public testing::Test {
 public:
  void SetUp() override {
    state_ = ThreadState::Current();
    ClearOutOldGarbage();
    initial_gc_age_ = state_->GcAge();
  }

  void TearDown() override {
    features_backup_.Restore();
    PreciselyCollectGarbage();
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    EXPECT_FALSE(state_->IsMarkingInProgress());
    EXPECT_FALSE(state_->IsSweepingInProgress());
  }

  void StartIncrementalMarkingForIdleGC() {
    RuntimeEnabledFeatures::SetHeapIncrementalMarkingEnabled(true);
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    state_->ScheduleIdleGC();
    RunIdleGCTask();
    EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
              state_->GetGCState());
    EXPECT_TRUE(state_->IsMarkingInProgress());
  }

  void RunIdleGCTask() {
    // Simulate running idle GC task, instead of actually running the posted
    // task.
    EXPECT_EQ(ThreadState::kIdleGCScheduled, state_->GetGCState());
    state_->PerformIdleGC(TimeTicks::Max());
  }

  void StartLazySweepingForPreciseGC() {
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    state_->SchedulePreciseGC();
    EXPECT_EQ(ThreadState::kPreciseGCScheduled, state_->GetGCState());
    RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
    EXPECT_TRUE(state_->IsSweepingInProgress());
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
  }

  void RunScheduledGC(BlinkGC::StackState stack_state) {
    state_->RunScheduledGC(stack_state);
  }

  // Counter that is incremented when sweep finishes.
  int GCCount() { return state_->GcAge() - initial_gc_age_; }

  ThreadState* state() { return state_; }

 private:
  ThreadState* state_;
  int initial_gc_age_;
  RuntimeEnabledFeatures::Backup features_backup_;
};

TEST_F(ThreadStateSchedulingTest, ScheduleIdleGCAgain) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIdleGC();
  EXPECT_EQ(ThreadState::kIdleGCScheduled, test->state()->GetGCState());

  // Calling ScheduleIdleGC() while an idle GC is scheduled will do nothing.
  test->state()->ScheduleIdleGC();

  EXPECT_EQ(ThreadState::kIdleGCScheduled, test->state()->GetGCState());
  EXPECT_EQ(0, test->GCCount());
}

TEST_F(ThreadStateSchedulingTest, ScheduleIncrementalV8FollowupGCAgain) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIncrementalGC(BlinkGC::kIncrementalV8FollowupGC);
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());

  // Calling ScheduleIncrementalV8FollowupGC() while one is already scheduled
  // will do nothing.
  test->state()->ScheduleIncrementalGC(BlinkGC::kIncrementalV8FollowupGC);

  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());
  EXPECT_EQ(0, test->GCCount());
}

TEST_F(ThreadStateSchedulingTest, ScheduleIdleGCWhileIncrementalMarking) {
  ThreadStateSchedulingTest* test = this;

  test->StartIncrementalMarkingForIdleGC();

  EXPECT_TRUE(test->state()->IsMarkingInProgress());
  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());

  // Calling ScheduleIdleGC() while an idle GC is scheduled should do nothing.
  test->state()->ScheduleIdleGC();

  EXPECT_TRUE(test->state()->IsMarkingInProgress());
  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, ScheduleIdleGCWhileLazySweeping) {
  ThreadStateSchedulingTest* test = this;

  test->StartLazySweepingForPreciseGC();

  test->state()->ScheduleIdleGC();

  // Scheduling an idle GC should finish lazy sweeping.
  EXPECT_FALSE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kIdleGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, SchedulePreciseGCWhileLazySweeping) {
  ThreadStateSchedulingTest* test = this;

  test->StartLazySweepingForPreciseGC();

  test->state()->SchedulePreciseGC();

  // Scheduling a precise GC should finish lazy sweeping.
  EXPECT_FALSE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kPreciseGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest,
       ScheduleIncrementalV8FollowupGCWhileLazySweeping) {
  ThreadStateSchedulingTest* test = this;

  test->StartLazySweepingForPreciseGC();

  test->state()->ScheduleIncrementalGC(BlinkGC::kIncrementalV8FollowupGC);

  // Scheduling a IncrementalV8FollowupGC should finish lazy sweeping.
  EXPECT_FALSE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, SchedulePreciseGCWhileIncrementalMarking) {
  ThreadStateSchedulingTest* test = this;

  test->StartIncrementalMarkingForIdleGC();

  test->state()->SchedulePreciseGC();

  // Scheduling a precise GC should cancel incremental marking tasks.
  EXPECT_EQ(ThreadState::kPreciseGCScheduled, test->state()->GetGCState());

  EXPECT_EQ(0, test->GCCount());
  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
  EXPECT_TRUE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());

  // Running the precise GC should simply finish the incremental marking idle GC
  // (not run another GC).
  EXPECT_EQ(0, test->GCCount());
  test->state()->CompleteSweep();
  EXPECT_EQ(1, test->GCCount());
}

TEST_F(ThreadStateSchedulingTest,
       ScheduleIncrementalV8FollowupGCWhileIncrementalMarking) {
  ThreadStateSchedulingTest* test = this;

  test->StartIncrementalMarkingForIdleGC();

  test->state()->ScheduleIncrementalGC(BlinkGC::kIncrementalV8FollowupGC);

  // Scheduling a precise GC should not cancel incremental marking tasks.
  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, ScheduleIdleGCWhileGCForbidden) {
  ThreadStateSchedulingTest* test = this;

  test->state()->ScheduleIdleGC();
  EXPECT_EQ(ThreadState::kIdleGCScheduled, test->state()->GetGCState());

  ThreadState::GCForbiddenScope gc_forbidden_scope(test->state());
  test->RunIdleGCTask();

  // Starting an idle GC while GC is forbidden should reschedule it.
  EXPECT_EQ(ThreadState::kIdleGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest,
       ScheduleIncrementalV8FollowupGCWhileGCForbidden) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIncrementalGC(BlinkGC::kIncrementalV8FollowupGC);
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());

  ThreadState::GCForbiddenScope gc_forbidden_scope(test->state());
  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);

  // Starting an IncrementalV8FollowupGC while GC is forbidden should do
  // nothing.
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());
  EXPECT_EQ(0, GCCount());
}

TEST_F(ThreadStateSchedulingTest, RunIncrementalV8FollowupGC) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->ScheduleIncrementalGC(BlinkGC::kIncrementalV8FollowupGC);
  EXPECT_EQ(ThreadState::kIncrementalGCScheduled, test->state()->GetGCState());

  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);

  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());
}

}  // namespace blink
