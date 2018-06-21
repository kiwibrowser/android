// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/skipped_frame_tracker.h"

#include <cmath>
#include "ui/latency/frame_metrics.h"

namespace ui {

SkippedFrameTracker::SkippedFrameTracker(Client* client) : client_(client) {}

void SkippedFrameTracker::BeginFrame(base::TimeTicks frame_time,
                                     base::TimeDelta interval) {
  DCHECK(!inside_begin_frame_);
  inside_begin_frame_ = true;
  did_produce_this_frame_ = false;
  frame_time_ = frame_time;
  interval_ = interval;

  // On our first frame of activity, we may need to initialize
  // will_produce_frame_time_.
  if (active_state_ == ActiveState::WillProduceFirst &&
      will_produce_frame_time_.is_null()) {
    will_produce_frame_time_ = frame_time_;
  }
}

void SkippedFrameTracker::FinishFrame() {
  DCHECK(inside_begin_frame_);
  inside_begin_frame_ = false;

  // Assume the source is idle if it hasn't attempted to produce for an entire
  // BeginFrame.
  if (!did_produce_this_frame_ && active_state_ == ActiveState::WasActive) {
    will_produce_frame_time_ = base::TimeTicks();
    active_state_ = ActiveState::Idle;
  }
}

void SkippedFrameTracker::WillProduceFrame() {
  // Make sure we don't transition out of the WillProduceFirst state until
  // we've actually produced the first frame.
  if (active_state_ == ActiveState::WillProduceFirst)
    return;

  // This is our first frame of activity.
  if (active_state_ == ActiveState::Idle) {
    active_state_ = ActiveState::WillProduceFirst;
    // If we're already inside a BeginFrame when we first become active,
    // we can initialize will_produce_frame_time_.
    if (inside_begin_frame_)
      will_produce_frame_time_ = frame_time_;
    return;
  }

  active_state_ = ActiveState::WillProduce;
}

void SkippedFrameTracker::DidProduceFrame() {
  // Ignore duplicate calls to DidProduceFrame.
  if (did_produce_this_frame_)
    return;

  // Return early if frame was pulled by sink.
  bool frame_was_pushed_by_source =
      (active_state_ == ActiveState::WillProduceFirst &&
       !will_produce_frame_time_.is_null()) ||
      active_state_ == ActiveState::WillProduce;
  if (!frame_was_pushed_by_source)
    return;

  DCHECK(!will_produce_frame_time_.is_null());

  // Clamp the amount of time skipped to a positive value, since negative
  // values aren't meaningful.
  base::TimeDelta skipped_clamped =
      std::max(base::TimeDelta(), (frame_time_ - will_produce_frame_time_));

  // Snap the amount of time skipped to whole intervals in order to filter
  // out jitter in the timing received by the BeginFrame source.
  int skipped_intervals = (skipped_clamped + (interval_ / 2)) / interval_;
  base::TimeDelta skipped_snapped = skipped_intervals * interval_;

  DCHECK_GE(skipped_snapped, base::TimeDelta());
  client_->AddFrameProduced(frame_time_, interval_, skipped_snapped);

  // Predict the next BeginFrame's frame time, so we can detect if it gets
  // dropped.
  will_produce_frame_time_ = frame_time_ + interval_;
  active_state_ = ActiveState::WasActive;
  did_produce_this_frame_ = true;
}

}  // namespace ui
