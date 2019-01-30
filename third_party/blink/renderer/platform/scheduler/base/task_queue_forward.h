// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_FORWARD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_FORWARD_H_

// Currently the implementation is located in Blink because scheduler/base move
// is in progress https://crbug.com/783309.
// TODO(kraynov): Remove this header after the move.

// Must be included first.
#include "third_party/blink/renderer/platform/platform_export.h"

#define HACKDEF_INCLUDED_FROM_BLINK
#include "base/task/sequence_manager/task_queue.h"
#undef HACKDEF_INCLUDED_FROM_BLINK

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_BASE_TASK_QUEUE_FORWARD_H_
