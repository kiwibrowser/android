/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/heap/thread_state.h"

#include <v8.h>

#include <algorithm>
#include <iomanip>
#include <limits>
#include <memory>

#include "base/atomicops.h"
#include "base/location.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/page_pool.h"
#include "third_party/blink/renderer/platform/heap/safe_point.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/stack_util.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

#if defined(OS_WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#endif

#if defined(MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#endif

#if defined(OS_FREEBSD)
#include <pthread_np.h>
#endif

namespace blink {

WTF::ThreadSpecific<ThreadState*>* ThreadState::thread_specific_ = nullptr;
uint8_t ThreadState::main_thread_state_storage_[sizeof(ThreadState)];

namespace {

const size_t kDefaultAllocatedObjectSizeThreshold = 100 * 1024;

// Duration of one incremental marking step. Should be short enough that it
// doesn't cause jank even though it is scheduled as a normal task.
constexpr TimeDelta kIncrementalMarkingStepDuration =
    TimeDelta::FromMilliseconds(1);

constexpr size_t kMaxTerminationGCLoops = 20;

const char* GcReasonString(BlinkGC::GCReason reason) {
  switch (reason) {
    case BlinkGC::kIdleGC:
      return "IdleGC";
    case BlinkGC::kPreciseGC:
      return "PreciseGC";
    case BlinkGC::kConservativeGC:
      return "ConservativeGC";
    case BlinkGC::kForcedGC:
      return "ForcedGC";
    case BlinkGC::kMemoryPressureGC:
      return "MemoryPressureGC";
    case BlinkGC::kPageNavigationGC:
      return "PageNavigationGC";
    case BlinkGC::kThreadTerminationGC:
      return "ThreadTerminationGC";
    case BlinkGC::kTesting:
      return "TestingGC";
    case BlinkGC::kIncrementalIdleGC:
      return "IncrementalIdleGC";
    case BlinkGC::kIncrementalV8FollowupGC:
      return "IncrementalV8FollowupGC";
  }
  return "<Unknown>";
}

const char* MarkingTypeString(BlinkGC::MarkingType type) {
  switch (type) {
    case BlinkGC::kAtomicMarking:
      return "AtomicMarking";
    case BlinkGC::kIncrementalMarking:
      return "IncrementalMarking";
    case BlinkGC::kTakeSnapshot:
      return "TakeSnapshot";
  }
  return "<Unknown>";
}

const char* SweepingTypeString(BlinkGC::SweepingType type) {
  switch (type) {
    case BlinkGC::kLazySweeping:
      return "LazySweeping";
    case BlinkGC::kEagerSweeping:
      return "EagerSweeping";
  }
  return "<Unknown>";
}

const char* StackStateString(BlinkGC::StackState state) {
  switch (state) {
    case BlinkGC::kNoHeapPointersOnStack:
      return "NoHeapPointersOnStack";
    case BlinkGC::kHeapPointersOnStack:
      return "HeapPointersOnStack";
  }
  return "<Unknown>";
}

// Helper function to convert a byte count to a KB count, capping at
// INT_MAX if the number is larger than that.
constexpr size_t CappedSizeInKB(size_t size_in_bytes) {
  const size_t size_in_kb = size_in_bytes / 1024;
  const size_t limit = std::numeric_limits<int>::max();
  return size_in_kb > limit ? limit : size_in_kb;
}

}  // namespace

ThreadState::ThreadState()
    : thread_(CurrentThread()),
      persistent_region_(std::make_unique<PersistentRegion>()),
      weak_persistent_region_(std::make_unique<PersistentRegion>()),
      start_of_stack_(reinterpret_cast<intptr_t*>(WTF::GetStackStart())),
      end_of_stack_(reinterpret_cast<intptr_t*>(WTF::GetStackStart())),
      safe_point_scope_marker_(nullptr),
      sweep_forbidden_(false),
      no_allocation_count_(0),
      gc_forbidden_count_(0),
      mixins_being_constructed_count_(0),
      object_resurrection_forbidden_(false),
      in_atomic_pause_(false),
      gc_mixin_marker_(nullptr),
      gc_state_(kNoGCScheduled),
      gc_phase_(GCPhase::kNone),
      reason_for_scheduled_gc_(BlinkGC::kLastGCReason),
      isolate_(nullptr),
      trace_dom_wrappers_(nullptr),
      invalidate_dead_objects_in_wrappers_marking_deque_(nullptr),
      perform_cleanup_(nullptr),
      wrapper_tracing_(false),
      incremental_marking_(false),
#if defined(ADDRESS_SANITIZER)
      asan_fake_stack_(__asan_get_current_fake_stack()),
#endif
#if defined(LEAK_SANITIZER)
      disabled_static_persistent_registration_(0),
#endif
      reported_memory_to_v8_(0) {
  DCHECK(CheckThread());
  DCHECK(!**thread_specific_);
  **thread_specific_ = this;

  heap_ = std::make_unique<ThreadHeap>(this);
}

ThreadState::~ThreadState() {
  DCHECK(CheckThread());
  if (IsMainThread())
    DCHECK_EQ(0u, Heap().stats_collector()->allocated_space_bytes());
  CHECK(GetGCState() == ThreadState::kNoGCScheduled);

  **thread_specific_ = nullptr;
}

void ThreadState::AttachMainThread() {
  thread_specific_ = new WTF::ThreadSpecific<ThreadState*>();
  new (main_thread_state_storage_) ThreadState();
}

void ThreadState::AttachCurrentThread() {
  new ThreadState();
}

void ThreadState::DetachCurrentThread() {
  ThreadState* state = Current();
  DCHECK(!state->IsMainThread());
  state->RunTerminationGC();
  delete state;
}

void ThreadState::RunTerminationGC() {
  DCHECK(!IsMainThread());
  DCHECK(CheckThread());

  if (IsMarkingInProgress()) {
    IncrementalMarkingFinalize();
  }

  // Finish sweeping.
  CompleteSweep();

  ReleaseStaticPersistentNodes();

  // PrepareForThreadStateTermination removes strong references so no need to
  // call it on CrossThreadWeakPersistentRegion.
  ProcessHeap::GetCrossThreadPersistentRegion()
      .PrepareForThreadStateTermination(this);

  // Do thread local GC's as long as the count of thread local Persistents
  // changes and is above zero.
  int old_count = -1;
  int current_count = GetPersistentRegion()->NumberOfPersistents();
  DCHECK_GE(current_count, 0);
  while (current_count != old_count) {
    CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                   BlinkGC::kEagerSweeping, BlinkGC::kThreadTerminationGC);
    // Release the thread-local static persistents that were
    // instantiated while running the termination GC.
    ReleaseStaticPersistentNodes();
    old_count = current_count;
    current_count = GetPersistentRegion()->NumberOfPersistents();
  }

  // We should not have any persistents left when getting to this point,
  // if we have it is a bug, and we have a reference cycle or a missing
  // RegisterAsStaticReference. Clearing out all the Persistents will avoid
  // stale pointers and gets them reported as nullptr dereferences.
  if (current_count) {
    for (size_t i = 0; i < kMaxTerminationGCLoops &&
                       GetPersistentRegion()->NumberOfPersistents();
         i++) {
      GetPersistentRegion()->PrepareForThreadStateTermination();
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kEagerSweeping, BlinkGC::kThreadTerminationGC);
    }
  }

  CHECK(!GetPersistentRegion()->NumberOfPersistents());

  // All of pre-finalizers should be consumed.
  DCHECK(ordered_pre_finalizers_.IsEmpty());
  CHECK_EQ(GetGCState(), kNoGCScheduled);

  Heap().RemoveAllPages();
}

NO_SANITIZE_ADDRESS
void ThreadState::VisitAsanFakeStackForPointer(MarkingVisitor* visitor,
                                               Address ptr) {
#if defined(ADDRESS_SANITIZER)
  Address* start = reinterpret_cast<Address*>(start_of_stack_);
  Address* end = reinterpret_cast<Address*>(end_of_stack_);
  Address* fake_frame_start = nullptr;
  Address* fake_frame_end = nullptr;
  Address* maybe_fake_frame = reinterpret_cast<Address*>(ptr);
  Address* real_frame_for_fake_frame = reinterpret_cast<Address*>(
      __asan_addr_is_in_fake_stack(asan_fake_stack_, maybe_fake_frame,
                                   reinterpret_cast<void**>(&fake_frame_start),
                                   reinterpret_cast<void**>(&fake_frame_end)));
  if (real_frame_for_fake_frame) {
    // This is a fake frame from the asan fake stack.
    if (real_frame_for_fake_frame > end && start > real_frame_for_fake_frame) {
      // The real stack address for the asan fake frame is
      // within the stack range that we need to scan so we need
      // to visit the values in the fake frame.
      for (Address* p = fake_frame_start; p < fake_frame_end; ++p)
        heap_->CheckAndMarkPointer(visitor, *p);
    }
  }
#endif
}

// Stack scanning may overrun the bounds of local objects and/or race with
// other threads that use this stack.
NO_SANITIZE_ADDRESS
NO_SANITIZE_THREAD
void ThreadState::VisitStack(MarkingVisitor* visitor) {
  if (stack_state_ == BlinkGC::kNoHeapPointersOnStack)
    return;

  Address* start = reinterpret_cast<Address*>(start_of_stack_);
  // If there is a safepoint scope marker we should stop the stack
  // scanning there to not touch active parts of the stack. Anything
  // interesting beyond that point is in the safepoint stack copy.
  // If there is no scope marker the thread is blocked and we should
  // scan all the way to the recorded end stack pointer.
  Address* end = reinterpret_cast<Address*>(end_of_stack_);
  Address* safe_point_scope_marker =
      reinterpret_cast<Address*>(safe_point_scope_marker_);
  Address* current = safe_point_scope_marker ? safe_point_scope_marker : end;

  // Ensure that current is aligned by address size otherwise the loop below
  // will read past start address.
  current = reinterpret_cast<Address*>(reinterpret_cast<intptr_t>(current) &
                                       ~(sizeof(Address) - 1));

  for (; current < start; ++current) {
    Address ptr = *current;
#if defined(MEMORY_SANITIZER)
    // |ptr| may be uninitialized by design. Mark it as initialized to keep
    // MSan from complaining.
    // Note: it may be tempting to get rid of |ptr| and simply use |current|
    // here, but that would be incorrect. We intentionally use a local
    // variable because we don't want to unpoison the original stack.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    heap_->CheckAndMarkPointer(visitor, ptr);
    VisitAsanFakeStackForPointer(visitor, ptr);
  }

  for (Address ptr : safe_point_stack_copy_) {
#if defined(MEMORY_SANITIZER)
    // See the comment above.
    __msan_unpoison(&ptr, sizeof(ptr));
#endif
    heap_->CheckAndMarkPointer(visitor, ptr);
    VisitAsanFakeStackForPointer(visitor, ptr);
  }
}

void ThreadState::VisitPersistents(Visitor* visitor) {
  {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kVisitCrossThreadPersistents);
    // See ProcessHeap::CrossThreadPersistentMutex().
    RecursiveMutexLocker persistent_lock(
        ProcessHeap::CrossThreadPersistentMutex());
    ProcessHeap::GetCrossThreadPersistentRegion().TracePersistentNodes(visitor);
  }
  {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitPersistents);
    persistent_region_->TracePersistentNodes(visitor);
  }
  if (trace_dom_wrappers_) {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kVisitDOMWrappers);
    trace_dom_wrappers_(isolate_, visitor);
  }
}

void ThreadState::VisitWeakPersistents(Visitor* visitor) {
  ProcessHeap::GetCrossThreadWeakPersistentRegion().TracePersistentNodes(
      visitor);
  weak_persistent_region_->TracePersistentNodes(visitor);
}

ThreadState::GCSnapshotInfo::GCSnapshotInfo(size_t num_object_types)
    : live_count(Vector<int>(num_object_types)),
      dead_count(Vector<int>(num_object_types)),
      live_size(Vector<size_t>(num_object_types)),
      dead_size(Vector<size_t>(num_object_types)) {}

size_t ThreadState::TotalMemorySize() {
  return heap_->stats_collector()->object_size_in_bytes() +
         WTF::Partitions::TotalSizeOfCommittedPages();
}

size_t ThreadState::EstimatedLiveSize(size_t estimation_base_size,
                                      size_t size_at_last_gc) {
  const ThreadHeapStatsCollector& stats_collector = *heap_->stats_collector();
  const ThreadHeapStatsCollector::Event& prev = stats_collector.previous();

  if (prev.wrapper_count_before_sweeping == 0)
    return estimation_base_size;

  // (estimated size) = (estimation base size) - (heap size at the last GC) /
  //   (# of persistent handles at the last GC) *
  //     (# of persistent handles collected since the last GC)
  size_t size_retained_by_collected_persistents = static_cast<size_t>(
      1.0 * size_at_last_gc / prev.wrapper_count_before_sweeping *
      stats_collector.collected_wrapper_count());
  if (estimation_base_size < size_retained_by_collected_persistents)
    return 0;
  return estimation_base_size - size_retained_by_collected_persistents;
}

double ThreadState::HeapGrowingRate() {
  const size_t current_size = heap_->stats_collector()->object_size_in_bytes();
  // TODO(mlippautz): Clarify those two parameters below.
  const size_t estimated_size =
      EstimatedLiveSize(heap_->stats_collector()->previous().marked_bytes,
                        heap_->stats_collector()->previous().marked_bytes);

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growing_rate =
      estimated_size > 0 ? 1.0 * current_size / estimated_size : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapEstimatedSizeKB",
                 CappedSizeInKB(estimated_size));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::heapGrowingRate",
                 static_cast<int>(100 * growing_rate));
  return growing_rate;
}

double ThreadState::PartitionAllocGrowingRate() {
  size_t current_size = WTF::Partitions::TotalSizeOfCommittedPages();
  size_t estimated_size = EstimatedLiveSize(
      current_size, heap_->stats_collector()
                        ->previous()
                        .partition_alloc_bytes_before_sweeping);

  // If the estimatedSize is 0, we set a high growing rate to trigger a GC.
  double growing_rate =
      estimated_size > 0 ? 1.0 * current_size / estimated_size : 100;
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocEstimatedSizeKB",
                 CappedSizeInKB(estimated_size));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "ThreadState::partitionAllocGrowingRate",
                 static_cast<int>(100 * growing_rate));
  return growing_rate;
}

// TODO(haraken): We should improve the GC heuristics. The heuristics affect
// performance significantly.
bool ThreadState::JudgeGCThreshold(size_t allocated_object_size_threshold,
                                   size_t total_memory_size_threshold,
                                   double heap_growing_rate_threshold) {
  // If the allocated object size or the total memory size is small, don't
  // trigger a GC.
  if (heap_->stats_collector()->allocated_bytes_since_prev_gc() <
          allocated_object_size_threshold ||
      TotalMemorySize() < total_memory_size_threshold)
    return false;

  VLOG(2) << "[state:" << this << "] JudgeGCThreshold:"
          << " heapGrowingRate=" << std::setprecision(1) << HeapGrowingRate()
          << " partitionAllocGrowingRate=" << std::setprecision(1)
          << PartitionAllocGrowingRate();
  // If the growing rate of Oilpan's heap or PartitionAlloc is high enough,
  // trigger a GC.
  return HeapGrowingRate() >= heap_growing_rate_threshold ||
         PartitionAllocGrowingRate() >= heap_growing_rate_threshold;
}

bool ThreadState::ShouldScheduleIdleGC() {
  if (GetGCState() != kNoGCScheduled)
    return false;
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold, 1024 * 1024,
                          1.5);
}

bool ThreadState::ShouldScheduleV8FollowupGC() {
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024, 1.5);
}

bool ThreadState::ShouldSchedulePageNavigationGC(
    float estimated_removal_ratio) {
  // If estimatedRemovalRatio is low we should let IdleGC handle this.
  if (estimated_removal_ratio < 0.01)
    return false;
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024,
                          1.5 * (1 - estimated_removal_ratio));
}

bool ThreadState::ShouldForceConservativeGC() {
  // TODO(haraken): 400% is too large. Lower the heap growing factor.
  return JudgeGCThreshold(kDefaultAllocatedObjectSizeThreshold,
                          32 * 1024 * 1024, 5.0);
}

// If we're consuming too much memory, trigger a conservative GC
// aggressively. This is a safe guard to avoid OOM.
bool ThreadState::ShouldForceMemoryPressureGC() {
  if (TotalMemorySize() < 300 * 1024 * 1024)
    return false;
  return JudgeGCThreshold(0, 0, 1.5);
}

void ThreadState::ScheduleV8FollowupGCIfNeeded(BlinkGC::V8GCType gc_type) {
  VLOG(2) << "[state:" << this << "] ScheduleV8FollowupGCIfNeeded: v8_gc_type="
          << ((gc_type == BlinkGC::kV8MajorGC) ? "MajorGC" : "MinorGC");
  DCHECK(CheckThread());

  if (IsGCForbidden())
    return;

  // This completeSweep() will do nothing in common cases since we've
  // called completeSweep() before V8 starts minor/major GCs.
  if (gc_type == BlinkGC::kV8MajorGC) {
    // TODO(ulan): Try removing this for Major V8 GC too.
    CompleteSweep();
    DCHECK(!IsSweepingInProgress());
    DCHECK(!SweepForbidden());
  }

  if ((gc_type == BlinkGC::kV8MajorGC && ShouldForceMemoryPressureGC()) ||
      ShouldScheduleV8FollowupGC()) {
    VLOG(2) << "[state:" << this << "] "
            << "ScheduleV8FollowupGCIfNeeded: Scheduled precise GC";
    SchedulePreciseGC();
    return;
  }
  if (gc_type == BlinkGC::kV8MajorGC && ShouldScheduleIdleGC()) {
    VLOG(2) << "[state:" << this << "] "
            << "ScheduleV8FollowupGCIfNeeded: Scheduled idle GC";
    ScheduleIdleGC();
    return;
  }
}

void ThreadState::WillStartV8GC(BlinkGC::V8GCType gc_type) {
  // Finish Oilpan's complete sweeping before running a V8 major GC.
  // This will let the GC collect more V8 objects.
  //
  // TODO(haraken): It's a bit too late for a major GC to schedule
  // completeSweep() here, because gcPrologue for a major GC is called
  // not at the point where the major GC started but at the point where
  // the major GC requests object grouping.
  DCHECK_EQ(BlinkGC::kV8MajorGC, gc_type);
  CompleteSweep();
}

void ThreadState::SchedulePageNavigationGCIfNeeded(
    float estimated_removal_ratio) {
  VLOG(2) << "[state:" << this << "] SchedulePageNavigationGCIfNeeded: "
          << "estimatedRemovalRatio=" << std::setprecision(2)
          << estimated_removal_ratio;
  DCHECK(CheckThread());

  if (IsGCForbidden())
    return;

  // Finish on-going lazy sweeping.
  // TODO(haraken): It might not make sense to force completeSweep() for all
  // page navigations.
  CompleteSweep();
  DCHECK(!IsSweepingInProgress());
  DCHECK(!SweepForbidden());

  if (ShouldForceMemoryPressureGC()) {
    VLOG(2) << "[state:" << this << "] "
            << "SchedulePageNavigationGCIfNeeded: Scheduled memory pressure GC";
    CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking,
                   BlinkGC::kLazySweeping, BlinkGC::kMemoryPressureGC);
    return;
  }
  if (ShouldSchedulePageNavigationGC(estimated_removal_ratio)) {
    VLOG(2) << "[state:" << this << "] "
            << "SchedulePageNavigationGCIfNeeded: Scheduled page navigation GC";
    SchedulePageNavigationGC();
  }
}

void ThreadState::SchedulePageNavigationGC() {
  DCHECK(CheckThread());
  DCHECK(!IsSweepingInProgress());
  SetGCState(kPageNavigationGCScheduled);
}

void ThreadState::ScheduleFullGC() {
  DCHECK(CheckThread());
  CompleteSweep();
  SetGCState(kFullGCScheduled);
}

void ThreadState::ScheduleGCIfNeeded() {
  VLOG(2) << "[state:" << this << "] ScheduleGCIfNeeded";
  DCHECK(CheckThread());

  // Allocation is allowed during sweeping, but those allocations should not
  // trigger nested GCs.
  if (IsGCForbidden() || SweepForbidden())
    return;

  ReportMemoryToV8();

  if (ShouldForceMemoryPressureGC()) {
    CompleteSweep();
    if (ShouldForceMemoryPressureGC()) {
      VLOG(2) << "[state:" << this << "] "
              << "ScheduleGCIfNeeded: Scheduled memory pressure GC";
      CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kLazySweeping, BlinkGC::kMemoryPressureGC);
      return;
    }
  }

  if (ShouldForceConservativeGC()) {
    CompleteSweep();
    if (ShouldForceConservativeGC()) {
      VLOG(2) << "[state:" << this << "] "
              << "ScheduleGCIfNeeded: Scheduled conservative GC";
      CollectGarbage(BlinkGC::kHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kLazySweeping, BlinkGC::kConservativeGC);
      return;
    }
  }

  if (ShouldScheduleIdleGC()) {
    VLOG(2) << "[state:" << this << "] "
            << "ScheduleGCIfNeeded: Scheduled idle GC";
    ScheduleIdleGC();
    return;
  }

#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
  if (GetGCState() == kNoGCScheduled &&
      RuntimeEnabledFeatures::HeapIncrementalMarkingStressEnabled()) {
    VLOG(2) << "[state:" << this << "] "
            << "ScheduleGCIfNeeded: Scheduled incremental marking for testing";
    IncrementalMarkingStart(BlinkGC::kTesting);
    return;
  }
#endif
}

ThreadState* ThreadState::FromObject(const void* object) {
  DCHECK(object);
  BasePage* page = PageFromObject(object);
  DCHECK(page);
  DCHECK(page->Arena());
  return page->Arena()->GetThreadState();
}

void ThreadState::PerformIdleGC(TimeTicks deadline) {
  DCHECK(CheckThread());
  DCHECK(Platform::Current()->CurrentThread()->Scheduler());

  if (GetGCState() != kIdleGCScheduled)
    return;

  if (IsGCForbidden()) {
    // If GC is forbidden at this point, try again.
    RescheduleIdleGC();
    return;
  }

  TimeDelta estimated_marking_time =
      heap_->stats_collector()->estimated_marking_time();
  if ((deadline - CurrentTimeTicks()) <= estimated_marking_time &&
      !Platform::Current()
           ->CurrentThread()
           ->Scheduler()
           ->CanExceedIdleDeadlineIfRequired()) {
    // If marking is estimated to take longer than the deadline and we can't
    // exceed the deadline, then reschedule for the next idle period.
    RescheduleIdleGC();
    return;
  }

#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
  if (RuntimeEnabledFeatures::HeapIncrementalMarkingEnabled()) {
    IncrementalMarkingStart(BlinkGC::kIncrementalIdleGC);
    return;
  }
#endif

  CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                 BlinkGC::kLazySweeping, BlinkGC::kIdleGC);
}

void ThreadState::PerformIdleLazySweep(TimeTicks deadline) {
  DCHECK(CheckThread());

  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // This check is here to prevent performIdleLazySweep() from being called
  // recursively. I'm not sure if it can happen but it would be safer to have
  // the check just in case.
  if (SweepForbidden())
    return;

  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kPerformIdleLazySweep);

  bool sweep_completed = false;
  {
    AtomicPauseScope atomic_pause_scope(this);
    SweepForbiddenScope scope(this);
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kLazySweepInIdle,
        "idleDeltaInSeconds", (deadline - CurrentTimeTicks()).InSecondsF());
    sweep_completed =
        Heap().AdvanceLazySweep(deadline.since_origin().InSecondsF());
    // We couldn't finish the sweeping within the deadline.
    // We request another idle task for the remaining sweeping.
    if (!sweep_completed)
      ScheduleIdleLazySweep();
  }

  if (sweep_completed)
    PostSweep();
}

void ThreadState::ScheduleIncrementalMarkingStep() {
  CHECK(!IsSweepingInProgress());
  SetGCState(kIncrementalMarkingStepScheduled);
}

void ThreadState::ScheduleIncrementalMarkingFinalize() {
  CHECK(!IsSweepingInProgress());
  SetGCState(kIncrementalMarkingFinalizeScheduled);
}

void ThreadState::ScheduleIdleGC() {
  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  // Also some tests can call Platform::SetCurrentPlatformForTesting() at any
  // time, so we need to check if it exists.
  if (!Platform::Current()->CurrentThread()->Scheduler())
    return;
  // Idle GC has the lowest priority so do not schedule if a GC is already
  // scheduled or if marking is in progress.
  if (GetGCState() != kNoGCScheduled)
    return;
  CompleteSweep();
  SetGCState(kIdleGCScheduled);
  Platform::Current()->CurrentThread()->Scheduler()->PostNonNestableIdleTask(
      FROM_HERE, WTF::Bind(&ThreadState::PerformIdleGC, WTF::Unretained(this)));
}

void ThreadState::RescheduleIdleGC() {
  DCHECK_EQ(kIdleGCScheduled, GetGCState());
  SetGCState(kNoGCScheduled);
  ScheduleIdleGC();
}

void ThreadState::ScheduleIdleLazySweep() {
  // Some threads (e.g. PPAPI thread) don't have a scheduler.
  if (!Platform::Current()->CurrentThread()->Scheduler())
    return;

  Platform::Current()->CurrentThread()->Scheduler()->PostIdleTask(
      FROM_HERE,
      WTF::Bind(&ThreadState::PerformIdleLazySweep, WTF::Unretained(this)));
}

void ThreadState::SchedulePreciseGC() {
  DCHECK(CheckThread());
  CompleteSweep();
  SetGCState(kPreciseGCScheduled);
}

void ThreadState::ScheduleIncrementalGC(BlinkGC::GCReason reason) {
  DCHECK(CheckThread());
  // Schedule an incremental GC only when no GC is scheduled or an idle GC is
  // scheduled. Otherwise, already scheduled GCs should be prioritized.
  if (GetGCState() == kNoGCScheduled || GetGCState() == kIdleGCScheduled) {
    CompleteSweep();
    reason_for_scheduled_gc_ = reason;
    SetGCState(kIncrementalGCScheduled);
  }
}

namespace {

#define UNEXPECTED_GCSTATE(s)                                   \
  case ThreadState::s:                                          \
    LOG(FATAL) << "Unexpected transition while in GCState " #s; \
    return

void UnexpectedGCState(ThreadState::GCState gc_state) {
  switch (gc_state) {
    UNEXPECTED_GCSTATE(kNoGCScheduled);
    UNEXPECTED_GCSTATE(kIdleGCScheduled);
    UNEXPECTED_GCSTATE(kPreciseGCScheduled);
    UNEXPECTED_GCSTATE(kFullGCScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingStepScheduled);
    UNEXPECTED_GCSTATE(kIncrementalMarkingFinalizeScheduled);
    UNEXPECTED_GCSTATE(kPageNavigationGCScheduled);
    UNEXPECTED_GCSTATE(kIncrementalGCScheduled);
  }
}

#undef UNEXPECTED_GCSTATE

}  // namespace

#define VERIFY_STATE_TRANSITION(condition) \
  if (UNLIKELY(!(condition)))              \
  UnexpectedGCState(gc_state_)

void ThreadState::SetGCState(GCState gc_state) {
  switch (gc_state) {
    case kNoGCScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(
          gc_state_ == kNoGCScheduled || gc_state_ == kIdleGCScheduled ||
          gc_state_ == kPreciseGCScheduled || gc_state_ == kFullGCScheduled ||
          gc_state_ == kPageNavigationGCScheduled ||
          gc_state_ == kIncrementalMarkingStepScheduled ||
          gc_state_ == kIncrementalMarkingFinalizeScheduled ||
          gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingStepScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIncrementalMarkingStepScheduled ||
                              gc_state_ == kIdleGCScheduled ||
                              gc_state_ == kIncrementalGCScheduled);
      break;
    case kIncrementalMarkingFinalizeScheduled:
      DCHECK(CheckThread());
      VERIFY_STATE_TRANSITION(gc_state_ == kIncrementalMarkingStepScheduled);
      break;
    case kFullGCScheduled:
    case kPageNavigationGCScheduled:
    case kPreciseGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(
          gc_state_ == kNoGCScheduled || gc_state_ == kIdleGCScheduled ||
          gc_state_ == kIncrementalMarkingStepScheduled ||
          gc_state_ == kIncrementalMarkingFinalizeScheduled ||
          gc_state_ == kPreciseGCScheduled || gc_state_ == kFullGCScheduled ||
          gc_state_ == kPageNavigationGCScheduled ||
          gc_state_ == kIncrementalGCScheduled);
      break;
    case kIdleGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsMarkingInProgress());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled);
      break;
    case kIncrementalGCScheduled:
      DCHECK(CheckThread());
      DCHECK(!IsMarkingInProgress());
      DCHECK(!IsSweepingInProgress());
      VERIFY_STATE_TRANSITION(gc_state_ == kNoGCScheduled ||
                              gc_state_ == kIdleGCScheduled);
      break;
    default:
      NOTREACHED();
  }
  gc_state_ = gc_state;
}

#undef VERIFY_STATE_TRANSITION

void ThreadState::SetGCPhase(GCPhase gc_phase) {
  switch (gc_phase) {
    case GCPhase::kNone:
      DCHECK_EQ(gc_phase_, GCPhase::kSweeping);
      break;
    case GCPhase::kMarking:
      DCHECK_EQ(gc_phase_, GCPhase::kNone);
      break;
    case GCPhase::kSweeping:
      DCHECK_EQ(gc_phase_, GCPhase::kMarking);
      break;
  }
  gc_phase_ = gc_phase;
}

void ThreadState::RunScheduledGC(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());
  if (stack_state != BlinkGC::kNoHeapPointersOnStack)
    return;

  // If a safe point is entered while initiating a GC, we clearly do
  // not want to do another as part of that -- the safe point is only
  // entered after checking if a scheduled GC ought to run first.
  // Prevent that from happening by marking GCs as forbidden while
  // one is initiated and later running.
  if (IsGCForbidden())
    return;

  switch (GetGCState()) {
    case kFullGCScheduled:
      CollectAllGarbage();
      break;
    case kPreciseGCScheduled:
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kLazySweeping, BlinkGC::kPreciseGC);
      break;
    case kPageNavigationGCScheduled:
      CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                     BlinkGC::kEagerSweeping, BlinkGC::kPageNavigationGC);
      break;
    case kIdleGCScheduled:
      // Idle time GC will be scheduled by Blink Scheduler.
      break;
    case kIncrementalMarkingStepScheduled:
      IncrementalMarkingStep();
      break;
    case kIncrementalMarkingFinalizeScheduled:
      IncrementalMarkingFinalize();
      break;
    case kIncrementalGCScheduled:
      IncrementalMarkingStart(reason_for_scheduled_gc_);
      break;
    default:
      break;
  }
}

void ThreadState::FinishSnapshot() {
  // Force setting NoGCScheduled to circumvent checkThread()
  // in setGCState().
  gc_state_ = kNoGCScheduled;
  SetGCPhase(GCPhase::kSweeping);
  SetGCPhase(GCPhase::kNone);
  Heap().stats_collector()->NotifySweepingCompleted();
}

void ThreadState::AtomicPauseEpilogue(BlinkGC::MarkingType marking_type,
                                      BlinkGC::SweepingType sweeping_type) {
  DCHECK(InAtomicMarkingPause());
  DCHECK(CheckThread());
  Heap().PrepareForSweep();

  if (marking_type == BlinkGC::kTakeSnapshot) {
    // Doing lazy sweeping for kTakeSnapshot doesn't make any sense so the
    // sweeping type should always be kEagerSweeping.
    DCHECK_EQ(sweeping_type, BlinkGC::kEagerSweeping);
    Heap().TakeSnapshot(ThreadHeap::SnapshotType::kHeapSnapshot);

    // This unmarks all marked objects and marks all unmarked objects dead.
    Heap().MakeConsistentForMutator();

    Heap().TakeSnapshot(ThreadHeap::SnapshotType::kFreelistSnapshot);
    return;
  }

  // We have to set the GCPhase to Sweeping before calling pre-finalizers
  // to disallow a GC during the pre-finalizers.
  SetGCPhase(GCPhase::kSweeping);

  // Allocation is allowed during the pre-finalizers and destructors.
  // However, they must not mutate an object graph in a way in which
  // a dead object gets resurrected.
  InvokePreFinalizers();

  EagerSweep();

  // Any sweep compaction must happen after pre-finalizers and eager
  // sweeping, as it will finalize dead objects in compactable arenas
  // (e.g., backing stores for container objects.)
  //
  // As per-contract for prefinalizers, those finalizable objects must
  // still be accessible when the prefinalizer runs, hence we cannot
  // schedule compaction until those have run. Similarly for eager sweeping.
  {
    SweepForbiddenScope scope(this);
    NoAllocationScope no_allocation_scope(this);
    Heap().Compact();
  }

#if defined(ADDRESS_SANITIZER)
  Heap().PoisonAllHeaps();
#endif
}

void ThreadState::EagerSweep() {
#if defined(ADDRESS_SANITIZER)
  Heap().PoisonEagerArena();
#endif
  DCHECK(CheckThread());
  // Some objects need to be finalized promptly and cannot be handled
  // by lazy sweeping. Keep those in a designated heap and sweep it
  // eagerly.
  DCHECK(IsSweepingInProgress());
  SweepForbiddenScope scope(this);
  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kEagerSweep);
  Heap().Arena(BlinkGC::kEagerSweepArenaIndex)->CompleteSweep();
}

void ThreadState::CompleteSweep() {
  DCHECK(CheckThread());
  // If we are not in a sweeping phase, there is nothing to do here.
  if (!IsSweepingInProgress())
    return;

  // completeSweep() can be called recursively if finalizers can allocate
  // memory and the allocation triggers completeSweep(). This check prevents
  // the sweeping from being executed recursively.
  if (SweepForbidden())
    return;

  {
    AtomicPauseScope atomic_pause_scope(this);
    SweepForbiddenScope scope(this);
    ThreadHeapStatsCollector::EnabledScope stats_scope(
        Heap().stats_collector(), ThreadHeapStatsCollector::kCompleteSweep);
    Heap().CompleteSweep();
  }
  PostSweep();
}

BlinkGCObserver::BlinkGCObserver(ThreadState* thread_state)
    : thread_state_(thread_state) {
  thread_state_->AddObserver(this);
}

BlinkGCObserver::~BlinkGCObserver() {
  thread_state_->RemoveObserver(this);
}

namespace {

// Update trace counters with statistics from the current and previous garbage
// collection cycle. We allow emitting current values here since these values
// can be useful for inspecting traces.
void UpdateTraceCounters(const ThreadHeapStatsCollector& stats_collector) {
  bool gc_tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                                     &gc_tracing_enabled);
  if (!gc_tracing_enabled)
    return;

  // Previous garbage collection cycle values.
  const ThreadHeapStatsCollector::Event& event = stats_collector.previous();
  const int collection_rate_percent =
      static_cast<int>(100 * (1.0 - event.live_object_rate));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.CollectionRate", collection_rate_percent);
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.MarkedObjectSizeAtLastCompleteSweepKB",
                 CappedSizeInKB(event.marked_bytes));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.ObjectSizeAtLastGCKB",
                 CappedSizeInKB(event.object_size_in_bytes_before_sweeping));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"), "BlinkGC.AllocatedSpaceAtLastGCKB",
      CappedSizeInKB(event.allocated_space_in_bytes_before_sweeping));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.PartitionAllocSizeAtLastGCKB",
                 CappedSizeInKB(event.partition_alloc_bytes_before_sweeping));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.WrapperCountAtLastGC",
                 event.wrapper_count_before_sweeping);

  // Current values.
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.AllocatedSpaceKB",
                 CappedSizeInKB(stats_collector.allocated_space_bytes()));
  TRACE_COUNTER1(
      TRACE_DISABLED_BY_DEFAULT("blink_gc"),
      "BlinkGC.AllocatedObjectSizeSincePreviousGCKB",
      CappedSizeInKB(stats_collector.allocated_bytes_since_prev_gc()));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "PartitionAlloc.TotalSizeOfCommittedPagesKB",
                 CappedSizeInKB(WTF::Partitions::TotalSizeOfCommittedPages()));
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"), "BlinkGC.WrapperCount",
                 stats_collector.wrapper_count());
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("blink_gc"),
                 "BlinkGC.CollectedWrapperCount",
                 stats_collector.collected_wrapper_count());
}

// Update histograms with statistics from the previous garbage collection cycle.
// Anything that is part of a histogram should have a well-defined lifetime wrt.
// to a garbage collection cycle.
void UpdateHistograms(const ThreadHeapStatsCollector::Event& event) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, gc_reason_histogram,
                      ("BlinkGC.GCReason", BlinkGC::kLastGCReason + 1));
  gc_reason_histogram.Count(event.reason);

  // TODO(mlippautz): Update name of this histogram.
  DEFINE_STATIC_LOCAL(CustomCountHistogram, marking_time_histogram,
                      ("BlinkGC.CollectGarbage", 0, 10 * 1000, 50));
  marking_time_histogram.Count(
      event.scope_data[ThreadHeapStatsCollector::kAtomicPhaseMarking]
          .InMilliseconds());

  DEFINE_STATIC_LOCAL(CustomCountHistogram, atomic_phase_marking_histogram,
                      ("BlinkGC.AtomicPhaseMarking", 0, 10 * 1000, 50));
  atomic_phase_marking_histogram.Count(
      event.scope_data[ThreadHeapStatsCollector::kAtomicPhaseMarking]
          .InMilliseconds());

  DEFINE_STATIC_LOCAL(CustomCountHistogram, complete_sweep_histogram,
                      ("BlinkGC.CompleteSweep", 1, 10 * 1000, 50));
  complete_sweep_histogram.Count(
      event.scope_data[ThreadHeapStatsCollector::kCompleteSweep]
          .InMilliseconds());

  DEFINE_STATIC_LOCAL(CustomCountHistogram, time_for_sweep_histogram,
                      ("BlinkGC.TimeForSweepingAllObjects", 1, 10 * 1000, 50));
  time_for_sweep_histogram.Count(event.sweeping_time_in_ms());

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, pre_finalizers_histogram,
      ("BlinkGC.TimeForInvokingPreFinalizers", 1, 10 * 1000, 50));
  pre_finalizers_histogram.Count(
      event.scope_data[ThreadHeapStatsCollector::kInvokePreFinalizers]
          .InMilliseconds());

  DEFINE_STATIC_LOCAL(CustomCountHistogram, time_for_heap_compaction_histogram,
                      ("BlinkGC.TimeForHeapCompaction", 1, 10 * 1000, 50));
  time_for_heap_compaction_histogram.Count(
      event.scope_data[ThreadHeapStatsCollector::kAtomicPhaseCompaction]
          .InMilliseconds());

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, object_size_freed_by_heap_compaction,
      ("BlinkGC.ObjectSizeFreedByHeapCompaction", 1, 4 * 1024 * 1024, 50));
  object_size_freed_by_heap_compaction.Count(
      CappedSizeInKB(event.compaction_freed_bytes));

  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, weak_processing_time_histogram,
      ("BlinkGC.TimeForGlobalWeakProcessing", 1, 10 * 1000, 50));
  weak_processing_time_histogram.Count(
      event.scope_data[ThreadHeapStatsCollector::kMarkWeakProcessing]
          .InMilliseconds());

  DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_before_gc_histogram,
                      ("BlinkGC.ObjectSizeBeforeGC", 1, 4 * 1024 * 1024, 50));
  object_size_before_gc_histogram.Count(
      CappedSizeInKB(event.object_size_in_bytes_before_sweeping));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, object_size_after_gc_histogram,
                      ("BlinkGC.ObjectSizeAfterGC", 1, 4 * 1024 * 1024, 50));
  object_size_after_gc_histogram.Count(CappedSizeInKB(event.marked_bytes));

  const int collection_rate_percent =
      static_cast<int>(100 * (1.0 - event.live_object_rate));
  DEFINE_STATIC_LOCAL(CustomCountHistogram, collection_rate_histogram,
                      ("BlinkGC.CollectionRate", 1, 100, 20));
  collection_rate_histogram.Count(collection_rate_percent);

  // Per GCReason metrics.
  switch (event.reason) {
#define COUNT_BY_GC_REASON(GCReason)                                        \
  case BlinkGC::k##GCReason: {                                              \
    DEFINE_STATIC_LOCAL(                                                    \
        CustomCountHistogram, atomic_marking_phase_histogram,               \
        ("BlinkGC.AtomicPhaseMarking_" #GCReason, 0, 10000, 50));           \
    atomic_marking_phase_histogram.Count(                                   \
        event.scope_data[ThreadHeapStatsCollector::kAtomicPhaseMarking]     \
            .InMilliseconds());                                             \
    DEFINE_STATIC_LOCAL(CustomCountHistogram, collection_rate_histogram,    \
                        ("BlinkGC.CollectionRate_" #GCReason, 1, 100, 20)); \
    collection_rate_histogram.Count(collection_rate_percent);               \
    break;                                                                  \
  }

    COUNT_BY_GC_REASON(IdleGC)
    COUNT_BY_GC_REASON(PreciseGC)
    COUNT_BY_GC_REASON(ConservativeGC)
    COUNT_BY_GC_REASON(ForcedGC)
    COUNT_BY_GC_REASON(MemoryPressureGC)
    COUNT_BY_GC_REASON(PageNavigationGC)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(Testing)
    COUNT_BY_GC_REASON(IncrementalIdleGC)
    COUNT_BY_GC_REASON(IncrementalV8FollowupGC)

#undef COUNT_BY_GC_REASON
  }

  static constexpr size_t kSupportedMaxSizeInMB = 4 * 1024;
  static size_t max_committed_size_in_mb = 0;

  // +1 for rounding up the size to the next MB.
  size_t size_in_mb =
      event.allocated_space_in_bytes_before_sweeping / 1024 / 1024 + 1;
  if (size_in_mb >= kSupportedMaxSizeInMB)
    size_in_mb = kSupportedMaxSizeInMB - 1;
  if (size_in_mb > max_committed_size_in_mb) {
    // Only update the counter for the maximum value.
    DEFINE_STATIC_LOCAL(EnumerationHistogram, commited_size_histogram,
                        ("BlinkGC.CommittedSize", kSupportedMaxSizeInMB));
    commited_size_histogram.Count(size_in_mb);
    max_committed_size_in_mb = size_in_mb;
  }
}

}  // namespace

void ThreadState::PostSweep() {
  DCHECK(CheckThread());

  SetGCPhase(GCPhase::kNone);
  if (GetGCState() == kIdleGCScheduled)
    ScheduleIdleGC();

  gc_age_++;

  for (auto* const observer : observers_)
    observer->OnCompleteSweepDone();

  Heap().stats_collector()->NotifySweepingCompleted();
  if (IsMainThread())
    UpdateHistograms(Heap().stats_collector()->previous());
  // Emit trace counters for all threads.
  UpdateTraceCounters(*Heap().stats_collector());
}

void ThreadState::SafePoint(BlinkGC::StackState stack_state) {
  DCHECK(CheckThread());

  RunScheduledGC(stack_state);
  stack_state_ = BlinkGC::kHeapPointersOnStack;
}

#ifdef ADDRESS_SANITIZER
// When we are running under AddressSanitizer with
// detect_stack_use_after_return=1 then stack marker obtained from
// SafePointScope will point into a fake stack.  Detect this case by checking if
// it falls in between current stack frame and stack start and use an arbitrary
// high enough value for it.  Don't adjust stack marker in any other case to
// match behavior of code running without AddressSanitizer.
NO_SANITIZE_ADDRESS static void* AdjustScopeMarkerForAdressSanitizer(
    void* scope_marker) {
  Address start = reinterpret_cast<Address>(WTF::GetStackStart());
  Address end = reinterpret_cast<Address>(&start);
  CHECK_LT(end, start);

  if (end <= scope_marker && scope_marker < start)
    return scope_marker;

  // 256 is as good an approximation as any else.
  const size_t kBytesToCopy = sizeof(Address) * 256;
  if (static_cast<size_t>(start - end) < kBytesToCopy)
    return start;

  return end + kBytesToCopy;
}
#endif

// TODO(haraken): The first void* pointer is unused. Remove it.
using PushAllRegistersCallback = void (*)(void*, ThreadState*, intptr_t*);
extern "C" void PushAllRegisters(void*, ThreadState*, PushAllRegistersCallback);

static void EnterSafePointAfterPushRegisters(void*,
                                             ThreadState* state,
                                             intptr_t* stack_end) {
  state->RecordStackEnd(stack_end);
  state->CopyStackUntilSafePointScope();
}

void ThreadState::EnterSafePoint(BlinkGC::StackState stack_state,
                                 void* scope_marker) {
  DCHECK(CheckThread());
#ifdef ADDRESS_SANITIZER
  if (stack_state == BlinkGC::kHeapPointersOnStack)
    scope_marker = AdjustScopeMarkerForAdressSanitizer(scope_marker);
#endif
  DCHECK(stack_state == BlinkGC::kNoHeapPointersOnStack || scope_marker);
  DCHECK(IsGCForbidden());
  stack_state_ = stack_state;
  safe_point_scope_marker_ = scope_marker;
  PushAllRegisters(nullptr, this, EnterSafePointAfterPushRegisters);
}

void ThreadState::LeaveSafePoint() {
  DCHECK(CheckThread());
  stack_state_ = BlinkGC::kHeapPointersOnStack;
  ClearSafePointScopeMarker();
}

void ThreadState::AddObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(observers_.find(observer) == observers_.end());
  observers_.insert(observer);
}

void ThreadState::RemoveObserver(BlinkGCObserver* observer) {
  DCHECK(observer);
  DCHECK(observers_.find(observer) != observers_.end());
  observers_.erase(observer);
}

void ThreadState::ReportMemoryToV8() {
  if (!isolate_)
    return;

  const size_t current_heap_size =
      heap_->stats_collector()->object_size_in_bytes();
  int64_t diff = static_cast<int64_t>(current_heap_size) -
                 static_cast<int64_t>(reported_memory_to_v8_);
  isolate_->AdjustAmountOfExternalAllocatedMemory(diff);
  reported_memory_to_v8_ = current_heap_size;
}

void ThreadState::CopyStackUntilSafePointScope() {
  if (!safe_point_scope_marker_ ||
      stack_state_ == BlinkGC::kNoHeapPointersOnStack)
    return;

  Address* to = reinterpret_cast<Address*>(safe_point_scope_marker_);
  Address* from = reinterpret_cast<Address*>(end_of_stack_);
  CHECK_LT(from, to);
  CHECK_LE(to, reinterpret_cast<Address*>(start_of_stack_));
  size_t slot_count = static_cast<size_t>(to - from);
// Catch potential performance issues.
#if defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
  // ASan/LSan use more space on the stack and we therefore
  // increase the allowed stack copying for those builds.
  DCHECK_LT(slot_count, 2048u);
#else
  DCHECK_LT(slot_count, 1024u);
#endif

  DCHECK(!safe_point_stack_copy_.size());
  safe_point_stack_copy_.resize(slot_count);
  for (size_t i = 0; i < slot_count; ++i) {
    safe_point_stack_copy_[i] = from[i];
  }
}

void ThreadState::RegisterStaticPersistentNode(
    PersistentNode* node,
    PersistentClearCallback callback) {
#if defined(LEAK_SANITIZER)
  if (disabled_static_persistent_registration_)
    return;
#endif

  DCHECK(!static_persistents_.Contains(node));
  static_persistents_.insert(node, callback);
}

void ThreadState::ReleaseStaticPersistentNodes() {
  HashMap<PersistentNode*, ThreadState::PersistentClearCallback>
      static_persistents;
  static_persistents.swap(static_persistents_);

  PersistentRegion* persistent_region = GetPersistentRegion();
  for (const auto& it : static_persistents)
    persistent_region->ReleasePersistentNode(it.key, it.value);
}

void ThreadState::FreePersistentNode(PersistentRegion* persistent_region,
                                     PersistentNode* persistent_node) {
  persistent_region->FreePersistentNode(persistent_node);
  // Do not allow static persistents to be freed before
  // they're all released in releaseStaticPersistentNodes().
  //
  // There's no fundamental reason why this couldn't be supported,
  // but no known use for it.
  if (persistent_region == GetPersistentRegion())
    DCHECK(!static_persistents_.Contains(persistent_node));
}

#if defined(LEAK_SANITIZER)
void ThreadState::enterStaticReferenceRegistrationDisabledScope() {
  disabled_static_persistent_registration_++;
}

void ThreadState::leaveStaticReferenceRegistrationDisabledScope() {
  DCHECK(disabled_static_persistent_registration_);
  disabled_static_persistent_registration_--;
}
#endif

void ThreadState::InvokePreFinalizers() {
  DCHECK(CheckThread());
  DCHECK(!SweepForbidden());

  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(), ThreadHeapStatsCollector::kInvokePreFinalizers);
  SweepForbiddenScope sweep_forbidden(this);
  // Pre finalizers may access unmarked objects but are forbidden from
  // ressurecting them.
  ObjectResurrectionForbiddenScope object_resurrection_forbidden(this);

  // Call the prefinalizers in the opposite order to their registration.
  //
  // LinkedHashSet does not support modification during iteration, so
  // copy items first.
  //
  // The prefinalizer callback wrapper returns |true| when its associated
  // object is unreachable garbage and the prefinalizer callback has run.
  // The registered prefinalizer entry must then be removed and deleted.
  Vector<PreFinalizer> reversed;
  for (auto rit = ordered_pre_finalizers_.rbegin();
       rit != ordered_pre_finalizers_.rend(); ++rit) {
    reversed.push_back(*rit);
  }
  for (PreFinalizer pre_finalizer : reversed) {
    if ((pre_finalizer.second)(pre_finalizer.first))
      ordered_pre_finalizers_.erase(pre_finalizer);
  }
}

// static
base::subtle::AtomicWord ThreadState::incremental_marking_counter_ = 0;

// static
base::subtle::AtomicWord ThreadState::wrapper_tracing_counter_ = 0;

void ThreadState::EnableIncrementalMarkingBarrier() {
  CHECK(!IsIncrementalMarking());
  base::subtle::Barrier_AtomicIncrement(&incremental_marking_counter_, 1);
  SetIncrementalMarking(true);
}

void ThreadState::DisableIncrementalMarkingBarrier() {
  CHECK(IsIncrementalMarking());
  base::subtle::Barrier_AtomicIncrement(&incremental_marking_counter_, -1);
  SetIncrementalMarking(false);
}

void ThreadState::EnableWrapperTracingBarrier() {
  CHECK(!IsWrapperTracing());
  base::subtle::Barrier_AtomicIncrement(&wrapper_tracing_counter_, 1);
  SetWrapperTracing(true);
}

void ThreadState::DisableWrapperTracingBarrier() {
  CHECK(IsWrapperTracing());
  base::subtle::Barrier_AtomicIncrement(&wrapper_tracing_counter_, -1);
  SetWrapperTracing(false);
}

void ThreadState::IncrementalMarkingStart(BlinkGC::GCReason reason) {
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Start";
  DCHECK(!IsMarkingInProgress());
  CompleteSweep();
  Heap().stats_collector()->NotifyMarkingStarted(reason);
  {
    ThreadHeapStatsCollector::Scope stats_scope(
        Heap().stats_collector(),
        ThreadHeapStatsCollector::kIncrementalMarkingStartMarking);
    AtomicPauseScope atomic_pause_scope(this);
    MarkPhasePrologue(BlinkGC::kNoHeapPointersOnStack,
                      BlinkGC::kIncrementalMarking, reason);
    MarkPhaseVisitRoots();
    EnableIncrementalMarkingBarrier();
    ScheduleIncrementalMarkingStep();
    DCHECK(IsMarkingInProgress());
  }
}

void ThreadState::IncrementalMarkingStep() {
  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingStep);
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Step";
  AtomicPauseScope atomic_pause_scope(this);
  DCHECK(IsMarkingInProgress());
  bool complete = MarkPhaseAdvanceMarking(CurrentTimeTicks() +
                                          kIncrementalMarkingStepDuration);
  if (complete)
    ScheduleIncrementalMarkingFinalize();
  else
    ScheduleIncrementalMarkingStep();
  DCHECK(IsMarkingInProgress());
}

void ThreadState::IncrementalMarkingFinalize() {
  ThreadHeapStatsCollector::Scope stats_scope(
      Heap().stats_collector(),
      ThreadHeapStatsCollector::kIncrementalMarkingFinalize);
  VLOG(2) << "[state:" << this << "] "
          << "IncrementalMarking: Finalize";
  // Call into the regular bottleneck instead of the internal version to get
  // UMA accounting and allow follow up GCs if necessary.
  CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kIncrementalMarking,
                 BlinkGC::kLazySweeping, current_gc_data_.reason);
}

void ThreadState::CollectGarbage(BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  // Nested garbage collection invocations are not supported.
  CHECK(!IsGCForbidden());
  // Garbage collection during sweeping is not supported. This can happen when
  // finalizers trigger garbage collections.
  if (SweepForbidden())
    return;

  TimeTicks start_total_collect_garbage_time = WTF::CurrentTimeTicks();
  RUNTIME_CALL_TIMER_SCOPE_IF_ISOLATE_EXISTS(
      GetIsolate(), RuntimeCallStats::CounterId::kCollectGarbage);

  const bool was_incremental_marking = IsMarkingInProgress();

  if (was_incremental_marking) {
    SetGCState(kNoGCScheduled);
    DisableIncrementalMarkingBarrier();
    DCHECK(IsMarkingInProgress());
    RunAtomicPause(stack_state, marking_type, sweeping_type, reason);
  }

  // We don't want floating garbage for the specific garbage collection types
  // mentioned below. In this case we will follow up with a regular full
  // garbage collection.
  const bool should_do_full_gc = !was_incremental_marking ||
                                 reason == BlinkGC::kForcedGC ||
                                 reason == BlinkGC::kMemoryPressureGC ||
                                 reason == BlinkGC::kThreadTerminationGC;
  if (should_do_full_gc) {
    CompleteSweep();
    SetGCState(kNoGCScheduled);
    Heap().stats_collector()->NotifyMarkingStarted(reason);
    RunAtomicPause(stack_state, marking_type, sweeping_type, reason);
  }

  const TimeDelta total_collect_garbage_time =
      WTF::CurrentTimeTicks() - start_total_collect_garbage_time;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, time_for_total_collect_garbage_histogram,
      ("BlinkGC.TimeForTotalCollectGarbage", 1, 10 * 1000, 50));
  time_for_total_collect_garbage_histogram.Count(
      total_collect_garbage_time.InMilliseconds());

#define COUNT_BY_GC_REASON(GCReason)                                      \
  case BlinkGC::k##GCReason: {                                            \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                      \
        CustomCountHistogram, histogram,                                  \
        ("BlinkGC.TimeForTotalCollectGarbage_" #GCReason, 0, 10000, 50)); \
    histogram.Count(total_collect_garbage_time.InMilliseconds());         \
    break;                                                                \
  }

  switch (reason) {
    COUNT_BY_GC_REASON(IdleGC)
    COUNT_BY_GC_REASON(PreciseGC)
    COUNT_BY_GC_REASON(ConservativeGC)
    COUNT_BY_GC_REASON(ForcedGC)
    COUNT_BY_GC_REASON(MemoryPressureGC)
    COUNT_BY_GC_REASON(PageNavigationGC)
    COUNT_BY_GC_REASON(ThreadTerminationGC)
    COUNT_BY_GC_REASON(Testing)
    COUNT_BY_GC_REASON(IncrementalIdleGC)
    COUNT_BY_GC_REASON(IncrementalV8FollowupGC)
  }
#undef COUNT_BY_GC_REASON

  VLOG(1) << "[state:" << this << "]"
          << " CollectGarbage: time: " << std::setprecision(2)
          << total_collect_garbage_time.InMillisecondsF() << "ms"
          << " stack: " << StackStateString(stack_state)
          << " marking: " << MarkingTypeString(marking_type)
          << " sweeping: " << SweepingTypeString(sweeping_type)
          << " reason: " << GcReasonString(reason);
}

void ThreadState::RunAtomicPause(BlinkGC::StackState stack_state,
                                 BlinkGC::MarkingType marking_type,
                                 BlinkGC::SweepingType sweeping_type,
                                 BlinkGC::GCReason reason) {
  {
    ThreadHeapStatsCollector::EnabledScope stats1(
        Heap().stats_collector(), ThreadHeapStatsCollector::kAtomicPhase);
    AtomicPauseScope atomic_pause_scope(this);
    {
      ThreadHeapStatsCollector::EnabledScope stats2(
          Heap().stats_collector(),
          ThreadHeapStatsCollector::kAtomicPhaseMarking, "lazySweeping",
          sweeping_type == BlinkGC::kLazySweeping ? "yes" : "no", "gcReason",
          GcReasonString(reason));
      AtomicPausePrologue(stack_state, marking_type, reason);
      MarkPhaseVisitRoots();
      CHECK(MarkPhaseAdvanceMarking(TimeTicks::Max()));
      MarkPhaseEpilogue(marking_type);
    }
    AtomicPauseEpilogue(marking_type, sweeping_type);
  }
  if (marking_type == BlinkGC::kTakeSnapshot) {
    FinishSnapshot();
    CHECK(!IsSweepingInProgress());
    CHECK_EQ(GetGCState(), kNoGCScheduled);
    return;
  }
  DCHECK(IsSweepingInProgress());
  if (sweeping_type == BlinkGC::kEagerSweeping) {
    // Eager sweeping should happen only in testing.
    CompleteSweep();
  } else {
    DCHECK(sweeping_type == BlinkGC::kLazySweeping);
    // The default behavior is lazy sweeping.
    ScheduleIdleLazySweep();
  }
}

namespace {

MarkingVisitor::MarkingMode GetMarkingMode(bool should_compact,
                                           bool create_snapshot) {
  CHECK(!should_compact || !create_snapshot);
  return (create_snapshot)
             ? MarkingVisitor::kSnapshotMarking
             : (should_compact) ? MarkingVisitor::kGlobalMarkingWithCompaction
                                : MarkingVisitor::kGlobalMarking;
}

}  // namespace

void ThreadState::MarkPhasePrologue(BlinkGC::StackState stack_state,
                                    BlinkGC::MarkingType marking_type,
                                    BlinkGC::GCReason reason) {
  SetGCPhase(GCPhase::kMarking);
  Heap().CommitCallbackStacks();

  const bool take_snapshot = marking_type == BlinkGC::kTakeSnapshot;
  const bool should_compact =
      !take_snapshot && Heap().Compaction()->ShouldCompact(
                            &Heap(), stack_state, marking_type, reason);

  current_gc_data_.visitor = MarkingVisitor::Create(
      this, GetMarkingMode(should_compact, take_snapshot));
  current_gc_data_.stack_state = stack_state;
  current_gc_data_.marking_type = marking_type;
  current_gc_data_.reason = reason;

  if (should_compact)
    Heap().Compaction()->Initialize(this);
}

void ThreadState::AtomicPausePrologue(BlinkGC::StackState stack_state,
                                      BlinkGC::MarkingType marking_type,
                                      BlinkGC::GCReason reason) {
  if (IsMarkingInProgress()) {
    // Incremental marking is already in progress. Only update the state
    // that is necessary to update.
    current_gc_data_.reason = reason;
    current_gc_data_.stack_state = stack_state;
    Heap().stats_collector()->UpdateReason(reason);
  } else {
    MarkPhasePrologue(stack_state, marking_type, reason);
  }

  if (marking_type == BlinkGC::kTakeSnapshot)
    BlinkGCMemoryDumpProvider::Instance()->ClearProcessDumpForCurrentGC();

  if (isolate_ && perform_cleanup_)
    perform_cleanup_(isolate_);

  DCHECK(InAtomicMarkingPause());
  Heap().MakeConsistentForGC();
  Heap().ClearArenaAges();
}

void ThreadState::MarkPhaseVisitRoots() {
  // StackFrameDepth should be disabled so we don't trace most of the object
  // graph in one incremental marking step.
  DCHECK(!Heap().GetStackFrameDepth().IsEnabled());

  // 1. Trace persistent roots.
  Heap().VisitPersistentRoots(current_gc_data_.visitor.get());

  // 2. Trace objects reachable from the stack.
  {
    SafePointScope safe_point_scope(current_gc_data_.stack_state, this);
    Heap().VisitStackRoots(current_gc_data_.visitor.get());
  }
}

bool ThreadState::MarkPhaseAdvanceMarking(TimeTicks deadline) {
  StackFrameDepthScope stack_depth_scope(&Heap().GetStackFrameDepth());
  // 3. Transitive closure to trace objects including ephemerons.
  return Heap().AdvanceMarkingStackProcessing(
      current_gc_data_.visitor.get(), deadline.since_origin().InSecondsF());
}

bool ThreadState::ShouldVerifyMarking() const {
  bool should_verify_marking =
      RuntimeEnabledFeatures::HeapIncrementalMarkingStressEnabled();
#if BUILDFLAG(BLINK_HEAP_VERIFICATION)
  should_verify_marking = true;
#endif  // BLINK_HEAP_VERIFICATION
  return should_verify_marking;
}

void ThreadState::MarkPhaseEpilogue(BlinkGC::MarkingType marking_type) {
  Visitor* visitor = current_gc_data_.visitor.get();
  // Finish marking of not-fully-constructed objects.
  Heap().MarkNotFullyConstructedObjects(visitor);
  CHECK(Heap().AdvanceMarkingStackProcessing(
      visitor, std::numeric_limits<double>::infinity()));

  {
    // See ProcessHeap::CrossThreadPersistentMutex().
    RecursiveMutexLocker persistent_lock(
        ProcessHeap::CrossThreadPersistentMutex());
    VisitWeakPersistents(visitor);
    Heap().WeakProcessing(visitor);
  }
  Heap().DecommitCallbackStacks();

  current_gc_data_.visitor.reset();

  if (ShouldVerifyMarking())
    VerifyMarking(marking_type);

  ProcessHeap::DecreaseTotalAllocatedObjectSize(
      Heap().stats_collector()->allocated_bytes_since_prev_gc());
  ProcessHeap::DecreaseTotalMarkedObjectSize(
      Heap().stats_collector()->previous().marked_bytes);
  Heap().stats_collector()->NotifyMarkingCompleted();
  WTF::Partitions::ReportMemoryUsageHistogram();

  if (invalidate_dead_objects_in_wrappers_marking_deque_)
    invalidate_dead_objects_in_wrappers_marking_deque_(isolate_);

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_object_space_histogram,
      ("BlinkGC.TotalObjectSpace", 0, 4 * 1024 * 1024, 50));
  total_object_space_histogram.Count(ProcessHeap::TotalAllocatedObjectSize() /
                                     1024);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, total_allocated_space_histogram,
      ("BlinkGC.TotalAllocatedSpace", 0, 4 * 1024 * 1024, 50));
  total_allocated_space_histogram.Count(ProcessHeap::TotalAllocatedSpace() /
                                        1024);
}

void ThreadState::VerifyMarking(BlinkGC::MarkingType marking_type) {
  // Marking for snapshot does not clear unreachable weak fields prohibiting
  // verification of markbits as we leave behind non-marked non-cleared weak
  // fields.
  if (marking_type == BlinkGC::kTakeSnapshot)
    return;
  Heap().VerifyMarking();
}

void ThreadState::CollectAllGarbage() {
  // We need to run multiple GCs to collect a chain of persistent handles.
  size_t previous_live_objects = 0;
  for (int i = 0; i < 5; ++i) {
    CollectGarbage(BlinkGC::kNoHeapPointersOnStack, BlinkGC::kAtomicMarking,
                   BlinkGC::kEagerSweeping, BlinkGC::kForcedGC);
    const size_t live_objects =
        Heap().stats_collector()->previous().marked_bytes;
    if (live_objects == previous_live_objects)
      break;
    previous_live_objects = live_objects;
  }
}

}  // namespace blink
