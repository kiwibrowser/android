// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_SEQUENCE_MANAGER_H_
#define BASE_TASK_SEQUENCE_MANAGER_SEQUENCE_MANAGER_H_

#include <memory>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/task_time_observer.h"

namespace base {
namespace sequence_manager {

class TimeDomain;

// SequenceManager manages TaskQueues which have different properties
// (e.g. priority, common task type) multiplexing all posted tasks into
// a single backing sequence (currently bound to a single thread, which is
// refererred as *main thread* in the comments below). SequenceManager
// implementation can be used in a various ways to apply scheduling logic.
class SequenceManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    // Called back on the main thread.
    virtual void OnBeginNestedRunLoop() = 0;
    virtual void OnExitNestedRunLoop() = 0;
  };

  virtual ~SequenceManager() = default;

  // TODO(kraynov): Bring back CreateOnCurrentThread static method here
  // when the move is done. It's not here yet to reduce PLATFORM_EXPORT
  // macros hacking during the move.

  // Must be called on the main thread.
  // Can be called only once, before creating TaskQueues.
  // Observer must outlive the SequenceManager.
  virtual void SetObserver(Observer* observer) = 0;

  // Must be called on the main thread.
  virtual void AddTaskObserver(MessageLoop::TaskObserver* task_observer) = 0;
  virtual void RemoveTaskObserver(MessageLoop::TaskObserver* task_observer) = 0;
  virtual void AddTaskTimeObserver(TaskTimeObserver* task_time_observer) = 0;
  virtual void RemoveTaskTimeObserver(TaskTimeObserver* task_time_observer) = 0;

  // Registers a TimeDomain with SequenceManager.
  // TaskQueues must only be created with a registered TimeDomain.
  // Conversely, any TimeDomain must remain registered until no
  // TaskQueues (using that TimeDomain) remain.
  virtual void RegisterTimeDomain(TimeDomain* time_domain) = 0;
  virtual void UnregisterTimeDomain(TimeDomain* time_domain) = 0;

  virtual TimeDomain* GetRealTimeDomain() const = 0;
  virtual const TickClock* GetTickClock() const = 0;
  virtual TimeTicks NowTicks() const = 0;

  // Sets the SingleThreadTaskRunner that will be returned by
  // ThreadTaskRunnerHandle::Get on the main thread.
  virtual void SetDefaultTaskRunner(
      scoped_refptr<SingleThreadTaskRunner> task_runner) = 0;

  // Removes all canceled delayed tasks.
  virtual void SweepCanceledDelayedTasks() = 0;

  // Returns true if no tasks were executed in TaskQueues that monitor
  // quiescence since the last call to this method.
  virtual bool GetAndClearSystemIsQuiescentBit() = 0;

  // Set the number of tasks executed in a single SequenceManager invocation.
  // Increasing this number reduces the overhead of the tasks dispatching
  // logic at the cost of a potentially worse latency. 1 by default.
  virtual void SetWorkBatchSize(int work_batch_size) = 0;

  // Enables crash keys that can be set in the scope of a task which help
  // to identify the culprit if upcoming work results in a crash.
  // Key names must be thread-specific to avoid races and corrupted crash dumps.
  virtual void EnableCrashKeys(const char* file_name_crash_key,
                               const char* function_name_crash_key) = 0;

  // Returns the portion of tasks for which CPU time is recorded or 0 if not
  // sampled.
  virtual double GetSamplingRateForRecordingCPUTime() const = 0;

  // Creates a task queue with the given type, |spec| and args.
  // Must be called on the main thread.
  // TODO(scheduler-dev): SequenceManager should not create TaskQueues.
  template <typename TaskQueueType, typename... Args>
  scoped_refptr<TaskQueueType> CreateTaskQueue(const TaskQueue::Spec& spec,
                                               Args&&... args) {
    return WrapRefCounted(new TaskQueueType(CreateTaskQueueImpl(spec), spec,
                                            std::forward<Args>(args)...));
  }

 protected:
  virtual std::unique_ptr<internal::TaskQueueImpl> CreateTaskQueueImpl(
      const TaskQueue::Spec& spec) = 0;
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_SEQUENCE_MANAGER_H_
