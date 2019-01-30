// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_TEST_TASK_TIME_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_TEST_TASK_TIME_OBSERVER_H_

#include "base/task/sequence_manager/task_time_observer.h"
#include "base/time/time.h"

namespace base {
namespace sequence_manager {

class TestTaskTimeObserver : public TaskTimeObserver {
 public:
  void WillProcessTask(base::TimeTicks start_time) override {}
  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override {}
};

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TEST_TEST_TASK_TIME_OBSERVER_H_
