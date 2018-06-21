// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_SKIPPED_FRAME_TRACKER_H_
#define UI_LATENCY_SKIPPED_FRAME_TRACKER_H_

#include "base/macros.h"
#include "base/time/time.h"

namespace ui {

// SkippedFrameTracker tracks skipped BeginFrames. It can be used by sources
// attempting to produce at the display rate. It properly handles
// non-consecutive BeginFrames and tracks when the source is actualy trying to
// produce, rather than passively receiving BeginFrames.
class SkippedFrameTracker {
 public:
  // SkippedFrameTracker calls Client::AddFrameProduced from FinishFrame
  // when necessary and with the correct values.
  class Client {
   public:
    virtual ~Client() = default;
    virtual void AddFrameProduced(base::TimeTicks source_timestamp,
                                  base::TimeDelta amount_produced,
                                  base::TimeDelta amount_skipped) = 0;
  };

  // SkippedFrameTracker will call |client|->AddFrameProduced
  // with the appropriate info automatically as frames are produced.
  explicit SkippedFrameTracker(Client* client);

  // BeginFrame and FinishFrame must be called for each BeginFrame received.
  // In order for this class to detect idle periods properly, the source must
  // call Begin+FinishFrame without calling WillProduceFrame before going
  // idle. This is necessary since there is otherwise no way to tell if a
  // non-consecutive BeginFrame occured a) because we were slow or b) because
  // we weren't trying to produce a frame.
  void BeginFrame(base::TimeTicks frame_time, base::TimeDelta interval);
  void FinishFrame();

  // WillProduceFrame should be called when the source knows it wants to
  // produce a frame. DidProduceFrame should be called when the source has
  // actually submitted the frame.
  // It is okay for DidProduceFrame to be called without WillProduceFrame,
  // which can happen in cases where a frame is "pulled" from later in the
  // pipeline rather than pushed from the source. Such calls to DidProduceFrame
  // will be ignored.
  void WillProduceFrame();
  void DidProduceFrame();

 protected:
  Client* client_;

  bool inside_begin_frame_ = false;
  base::TimeTicks frame_time_;
  base::TimeDelta interval_;
  bool did_produce_this_frame_ = false;

  base::TimeTicks will_produce_frame_time_;

  enum class ActiveState {
    // Idle: The initial and idle state.
    // Goto WillProduceFirst on 1st call to WillProduceFrame.
    Idle,
    // WillProduceFirst: Producing the first frame out of idle.
    // Goto WasActive on first FinishFrame after a DidProduceFrame.
    // Counts missing BeginFrames as skipped: NO.
    WillProduceFirst,
    // WillProduce: Producing the (N > 1)'th frame of constant activity.
    // Goto WasActive on first FinishFrame after a DidProduceFrame.
    // Counts missing BeginFrames as skipped: YES.
    WillProduce,
    // WasActive: An intermediate state to determine if we are idle or not.
    // Goto WillProduce on WillProduceFrame.
    // Otherwise, goto Idle on next FinishFrame.
    WasActive,
  };
  ActiveState active_state_ = ActiveState::Idle;

  DISALLOW_COPY_AND_ASSIGN(SkippedFrameTracker);
};

}  // namespace ui

#endif  // UI_LATENCY_SKIPPED_FRAME_TRACKER_H_
