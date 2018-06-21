// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

// =============================================================================
// ThreadHeapStatsCollector. ===================================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, InitialEmpty) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  for (int i = 0; i < ThreadHeapStatsCollector::kNumScopeIds; i++) {
    EXPECT_EQ(TimeDelta(), stats_collector.current().scope_data[i]);
  }
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, IncreaseScopeTime) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      TimeDelta::FromMilliseconds(1));
  EXPECT_EQ(TimeDelta::FromMilliseconds(1),
            stats_collector.current()
                .scope_data[ThreadHeapStatsCollector::kIncrementalMarkingStep]);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, StopMovesCurrentToPrevious) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      TimeDelta::FromMilliseconds(1));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(TimeDelta::FromMilliseconds(1),
            stats_collector.previous()
                .scope_data[ThreadHeapStatsCollector::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StopResetsCurrent) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      TimeDelta::FromMilliseconds(1));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(TimeDelta(),
            stats_collector.current()
                .scope_data[ThreadHeapStatsCollector::kIncrementalMarkingStep]);
}

TEST(ThreadHeapStatsCollectorTest, StartStop) {
  ThreadHeapStatsCollector stats_collector;
  EXPECT_FALSE(stats_collector.is_started());
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  EXPECT_TRUE(stats_collector.is_started());
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_FALSE(stats_collector.is_started());
}

TEST(ThreadHeapStatsCollectorTest, ScopeToString) {
  EXPECT_STREQ("BlinkGC.IncrementalMarkingStartMarking",
               ThreadHeapStatsCollector::ToString(
                   ThreadHeapStatsCollector::kIncrementalMarkingStartMarking));
}

TEST(ThreadHeapStatsCollectorTest, UpdateReason) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.UpdateReason(BlinkGC::kForcedGC);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(BlinkGC::kForcedGC, stats_collector.previous().reason);
}

TEST(ThreadHeapStatsCollectorTest, InitialEstimatedObjectSizeInBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  EXPECT_EQ(0u, stats_collector.object_size_in_bytes());
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedObjectSizeInBytesNoMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseAllocatedObjectSize(512);
  EXPECT_EQ(512u, stats_collector.object_size_in_bytes());
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedObjectSizeInBytesWithMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseAllocatedObjectSize(512);
  EXPECT_EQ(640u, stats_collector.object_size_in_bytes());
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest,
     EstimatedObjectSizeInBytesDoNotCountCurrentlyMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  // Currently marked bytes should not account to the estimated object size.
  stats_collector.IncreaseAllocatedObjectSize(512);
  EXPECT_EQ(640u, stats_collector.object_size_in_bytes());
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, PreInitializedEstimatedMarkingTime) {
  // Checks that a marking time estimate can be retrieved before the first
  // garbage collection triggers.
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  EXPECT_LT(0u, stats_collector.estimated_marking_time_in_seconds());
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedMarkingTime1) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPhaseMarking, TimeDelta::FromSeconds(1));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(1024);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  EXPECT_DOUBLE_EQ(1.0, stats_collector.estimated_marking_time_in_seconds());
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, EstimatedMarkingTime2) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPhaseMarking, TimeDelta::FromSeconds(1));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(1024);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseAllocatedObjectSize(512);
  EXPECT_DOUBLE_EQ(1.5, stats_collector.estimated_marking_time_in_seconds());
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
}

TEST(ThreadHeapStatsCollectorTest, AllocatedSpaceInBytesInitialZero) {
  ThreadHeapStatsCollector stats_collector;
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
  stats_collector.NotifyMarkingCompleted();
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
}

TEST(ThreadHeapStatsCollectorTest, AllocatedSpaceInBytesIncrease) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.IncreaseAllocatedSpace(1024);
  EXPECT_EQ(1024u, stats_collector.allocated_space_bytes());
}

TEST(ThreadHeapStatsCollectorTest, AllocatedSpaceInBytesDecrease) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.IncreaseAllocatedSpace(1024);
  stats_collector.DecreaseAllocatedSpace(1024);
  EXPECT_EQ(0u, stats_collector.allocated_space_bytes());
}

// =============================================================================
// ThreadHeapStatsCollector::Event. ============================================
// =============================================================================

TEST(ThreadHeapStatsCollectorTest, EventPrevGCMarkedObjectSize) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(1024);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(1024u, stats_collector.previous().marked_bytes);
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimeInMsFromIncrementalGC) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStartMarking,
      TimeDelta::FromMilliseconds(7));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingStep,
      TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingFinalizeMarking,
      TimeDelta::FromMilliseconds(1));
  // Ignore the full finalization.
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kIncrementalMarkingFinalize,
      TimeDelta::FromMilliseconds(3));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(10.0, stats_collector.previous().marking_time_in_ms());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimeInMsFromFullGC) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPhaseMarking,
      TimeDelta::FromMilliseconds(11));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(11.0, stats_collector.previous().marking_time_in_ms());
}

TEST(ThreadHeapStatsCollectorTest, EventMarkingTimePerByteInS) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseMarkedObjectSize(1000);
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kAtomicPhaseMarking, TimeDelta::FromSeconds(1));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(
      .001, stats_collector.previous().marking_time_in_bytes_per_second());
}

TEST(ThreadHeapStatsCollectorTest, EventSweepingTimeInMs) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kLazySweepInIdle,
                                    TimeDelta::FromMilliseconds(1));
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kLazySweepInIdle,
                                    TimeDelta::FromMilliseconds(2));
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kLazySweepInIdle,
                                    TimeDelta::FromMilliseconds(3));
  stats_collector.IncreaseScopeTime(
      ThreadHeapStatsCollector::kLazySweepOnAllocation,
      TimeDelta::FromMilliseconds(4));
  stats_collector.IncreaseScopeTime(ThreadHeapStatsCollector::kCompleteSweep,
                                    TimeDelta::FromMilliseconds(5));
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(15.0, stats_collector.previous().sweeping_time_in_ms());
}

TEST(ThreadHeapStatsCollectorTest, EventCompactionFreedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseCompactionFreedSize(512);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(512u, stats_collector.previous().compaction_freed_bytes);
}

TEST(ThreadHeapStatsCollectorTest, EventCompactionFreedPages) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseCompactionFreedPages(3);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(3u, stats_collector.previous().compaction_freed_pages);
}

TEST(ThreadHeapStatsCollectorTest, EventInitialEstimatedLiveObjectRate) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateSameMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(1.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateHalfMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(256);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0.5, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest, EventEstimatedLiveObjectRateNoMarkedBytes) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(256);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes1) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.IncreaseAllocatedObjectSize(128);
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(.5, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes2) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  stats_collector.IncreaseAllocatedObjectSize(128);
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(1.0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes3) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest,
     EventEstimatedLiveObjectRateWithAllocatedBytes4) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseMarkedObjectSize(128);
  stats_collector.NotifySweepingCompleted();
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.NotifySweepingCompleted();
  EXPECT_DOUBLE_EQ(0, stats_collector.previous().live_object_rate);
}

TEST(ThreadHeapStatsCollectorTest, EventAllocatedSpaceBeforeSweeping1) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseAllocatedSpace(1024);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.IncreaseAllocatedSpace(2048);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(
      1024u,
      stats_collector.previous().allocated_space_in_bytes_before_sweeping);
}

TEST(ThreadHeapStatsCollectorTest, EventAllocatedSpaceBeforeSweeping2) {
  ThreadHeapStatsCollector stats_collector;
  stats_collector.NotifyMarkingStarted(BlinkGC::kTesting);
  stats_collector.IncreaseAllocatedSpace(1024);
  stats_collector.NotifyMarkingCompleted();
  stats_collector.DecreaseAllocatedSpace(1024);
  stats_collector.NotifySweepingCompleted();
  EXPECT_EQ(
      1024u,
      stats_collector.previous().allocated_space_in_bytes_before_sweeping);
}

}  // namespace blink
