// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FORWARD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FORWARD_H_

// TODO(kraynov): Remove this header after the move.

// Must be included first.
#include "third_party/blink/renderer/platform/platform_export.h"

#define HACKDEF_INCLUDED_FROM_BLINK
// It also includes task_queue_impl.h which has PLATFORM_EXPORT macros.
#include "base/task/sequence_manager/sequence_manager.h"
#undef HACKDEF_INCLUDED_FROM_BLINK

namespace base {
namespace sequence_manager {

// Create SequenceManager using MessageLoop on the current thread.
// implementation is located in task_queue_manager_impl.cc.
// TODO(kraynov): Move to SequenceManager class.
// TODO(scheduler-dev): Rename to TakeOverCurrentThread when we'll stop using
// MessageLoop and will actually take over a thread.
PLATFORM_EXPORT std::unique_ptr<SequenceManager>
CreateSequenceManagerOnCurrentThread();

}  // namespace sequence_manager
}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_SEQUENCE_MANAGER_FORWARD_H_
