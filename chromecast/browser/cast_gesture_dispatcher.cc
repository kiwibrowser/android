// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_gesture_dispatcher.h"

#include "chromecast/base/chromecast_switches.h"

namespace chromecast {
namespace shell {

namespace {
constexpr int kDefaultBackGestureHorizontalThreshold = 80;
}  // namespace

CastGestureDispatcher::CastGestureDispatcher(
    CastContentWindow::Delegate* delegate)
    : horizontal_threshold_(
          GetSwitchValueInt(switches::kBackGestureHorizontalThreshold,
                            kDefaultBackGestureHorizontalThreshold)),
      delegate_(delegate),
      dispatched_back_(false) {
  DCHECK(delegate_);
}
bool CastGestureDispatcher::CanHandleSwipe(CastSideSwipeOrigin swipe_origin) {
  return swipe_origin == CastSideSwipeOrigin::LEFT &&
         delegate_->CanHandleGesture(GestureType::GO_BACK);
}

void CastGestureDispatcher::HandleSideSwipeBegin(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  if (swipe_origin == CastSideSwipeOrigin::LEFT) {
    dispatched_back_ = false;
    VLOG(1) << "swipe gesture begin";
    current_swipe_time_ = base::ElapsedTimer();
  }
}

void CastGestureDispatcher::HandleSideSwipeContinue(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  if (swipe_origin != CastSideSwipeOrigin::LEFT) {
    return;
  }

  if (!delegate_->CanHandleGesture(GestureType::GO_BACK)) {
    return;
  }

  delegate_->GestureProgress(GestureType::GO_BACK, touch_location);
  VLOG(1) << "swipe gesture continue, elapsed time: "
          << current_swipe_time_.Elapsed().InMilliseconds() << "ms";

  if (!dispatched_back_ && touch_location.x() >= horizontal_threshold_) {
    dispatched_back_ = true;
    delegate_->ConsumeGesture(GestureType::GO_BACK);
    VLOG(1) << "swipe gesture complete, elapsed time: "
            << current_swipe_time_.Elapsed().InMilliseconds() << "ms";
  }
}

void CastGestureDispatcher::HandleSideSwipeEnd(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  if (swipe_origin != CastSideSwipeOrigin::LEFT) {
    return;
  }
  VLOG(1) << "swipe end, elapsed time: "
          << current_swipe_time_.Elapsed().InMilliseconds() << "ms";
  if (!delegate_->CanHandleGesture(GestureType::GO_BACK)) {
    return;
  }
  if (!dispatched_back_ && touch_location.x() < horizontal_threshold_) {
    VLOG(1) << "swipe gesture cancelled";
    delegate_->CancelGesture(GestureType::GO_BACK, touch_location);
  }
}

void CastGestureDispatcher::HandleTapGesture(const gfx::Point& touch_location) {
  if (!delegate_->CanHandleGesture(GestureType::TAP)) {
    return;
  }
  delegate_->ConsumeGesture(GestureType::TAP);
}

}  // namespace shell
}  // namespace chromecast
