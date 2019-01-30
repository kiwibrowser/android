// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gesture_detector.h"

#include "base/numerics/math_constants.h"
#include "third_party/blink/public/platform/web_gesture_event.h"

namespace vr {

namespace {

constexpr float kDisplacementScaleFactor = 300.0f;

constexpr int kMaxNumOfExtrapolations = 2;

// Minimum time distance needed to call two timestamps
// not equal.
constexpr float kDelta = 1.0e-7f;

constexpr float kCutoffHz = 10.0f;
constexpr float kRC = 1.0f / (2.0f * base::kPiFloat * kCutoffHz);

// A slop represents a small rectangular region around the first touch point of
// a gesture.
// If the user does not move outside of the slop, no gesture is detected.
// Gestures start to be detected when the user moves outside of the slop.
// Vertical distance from the border to the center of slop.
constexpr float kSlopVertical = 0.165f;

// Horizontal distance from the border to the center of slop.
constexpr float kSlopHorizontal = 0.15f;

}  // namespace

GestureDetector::GestureDetector() {
  Reset();
}
GestureDetector::~GestureDetector() = default;

std::unique_ptr<GestureList> GestureDetector::DetectGestures(
    const TouchInfo& input_touch_info,
    base::TimeTicks current_timestamp,
    bool force_cancel) {
  touch_position_changed_ = UpdateCurrentTouchPoint(input_touch_info);
  TouchInfo touch_info = input_touch_info;
  ExtrapolateTouchInfo(&touch_info, current_timestamp);
  if (touch_position_changed_)
    UpdateOverallVelocity(touch_info);

  auto gesture_list = std::make_unique<GestureList>();
  auto gesture = GetGestureFromTouchInfo(touch_info, force_cancel);
  gesture->SetSourceDevice(blink::kWebGestureDeviceTouchpad);

  if (gesture->GetType() == blink::WebInputEvent::kGestureScrollEnd)
    Reset();

  if (gesture->GetType() != blink::WebInputEvent::kUndefined)
    gesture_list->push_back(std::move(gesture));
  return gesture_list;
}

std::unique_ptr<blink::WebGestureEvent>
GestureDetector::GetGestureFromTouchInfo(const TouchInfo& touch_info,
                                         bool force_cancel) {
  auto gesture = std::make_unique<blink::WebGestureEvent>();
  gesture->SetTimeStamp(touch_info.touch_point.timestamp);

  switch (state_->label) {
    // User has not put finger on touch pad.
    case WAITING:
      HandleWaitingState(touch_info, gesture.get());
      break;
    // User has not started a gesture (by moving out of slop).
    case TOUCHING:
      HandleDetectingState(touch_info, force_cancel, gesture.get());
      break;
    // User is scrolling on touchpad
    case SCROLLING:
      HandleScrollingState(touch_info, force_cancel, gesture.get());
      break;
    // The user has finished scrolling, but we'll hallucinate a few points
    // before really finishing.
    case POST_SCROLL:
      HandlePostScrollingState(touch_info, force_cancel, gesture.get());
      break;
    default:
      NOTREACHED();
      break;
  }
  return gesture;
}

void GestureDetector::HandleWaitingState(const TouchInfo& touch_info,
                                         blink::WebGestureEvent* gesture) {
  // User puts finger on touch pad (or when the touch down for current gesture
  // is missed, initiate gesture from current touch point).
  if (touch_info.touch_down || touch_info.is_touching) {
    // update initial touchpoint
    state_->initial_touch_point = touch_info.touch_point;
    // update current touchpoint
    state_->cur_touch_point = touch_info.touch_point;
    state_->label = TOUCHING;

    gesture->SetType(blink::WebInputEvent::kGestureFlingCancel);
    gesture->data.fling_cancel.prevent_boosting = false;
  }
}

void GestureDetector::HandleDetectingState(const TouchInfo& touch_info,
                                           bool force_cancel,
                                           blink::WebGestureEvent* gesture) {
  // User lifts up finger from touch pad.
  if (touch_info.touch_up || !touch_info.is_touching) {
    Reset();
    return;
  }

  // Touch position is changed, the touch point moves outside of slop,
  // and the Controller's button is not down.
  if (touch_position_changed_ && touch_info.is_touching &&
      !InSlop(touch_info.touch_point.position) && !force_cancel) {
    state_->label = SCROLLING;
    gesture->SetType(blink::WebInputEvent::kGestureScrollBegin);
    UpdateGestureParameters(touch_info);
    gesture->data.scroll_begin.delta_x_hint =
        state_->displacement.x() * kDisplacementScaleFactor;
    gesture->data.scroll_begin.delta_y_hint =
        state_->displacement.y() * kDisplacementScaleFactor;
    gesture->data.scroll_begin.delta_hint_units =
        blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  }
}

void GestureDetector::HandleScrollingState(const TouchInfo& touch_info,
                                           bool force_cancel,
                                           blink::WebGestureEvent* gesture) {
  if (force_cancel) {
    gesture->SetType(blink::WebInputEvent::kGestureScrollEnd);
    UpdateGestureParameters(touch_info);
    return;
  }
  if (touch_info.touch_up || !(touch_info.is_touching)) {
    state_->label = POST_SCROLL;
  }
  if (touch_position_changed_) {
    gesture->SetType(blink::WebInputEvent::kGestureScrollUpdate);
    UpdateGestureParameters(touch_info);
    UpdateGestureWithScrollDelta(gesture);
  }
}

void GestureDetector::HandlePostScrollingState(
    const TouchInfo& touch_info,
    bool force_cancel,
    blink::WebGestureEvent* gesture) {
  if (extrapolated_touch_ == 0 || force_cancel) {
    gesture->SetType(blink::WebInputEvent::kGestureScrollEnd);
    UpdateGestureParameters(touch_info);
  } else {
    gesture->SetType(blink::WebInputEvent::kGestureScrollUpdate);
    UpdateGestureParameters(touch_info);
    UpdateGestureWithScrollDelta(gesture);
  }
}

void GestureDetector::UpdateGestureWithScrollDelta(
    blink::WebGestureEvent* gesture) {
  gesture->data.scroll_update.delta_x =
      state_->displacement.x() * kDisplacementScaleFactor;
  gesture->data.scroll_update.delta_y =
      state_->displacement.y() * kDisplacementScaleFactor;
}

bool GestureDetector::UpdateCurrentTouchPoint(const TouchInfo& touch_info) {
  if (touch_info.is_touching || touch_info.touch_up) {
    // Update the touch point when the touch position has changed.
    if (state_->cur_touch_point.position != touch_info.touch_point.position) {
      state_->prev_touch_point = state_->cur_touch_point;
      state_->cur_touch_point = touch_info.touch_point;
      return true;
    }
  }
  return false;
}

void GestureDetector::ExtrapolateTouchInfo(TouchInfo* touch_info,
                                           base::TimeTicks current_timestamp) {
  const bool effectively_scrolling =
      state_->label == SCROLLING || state_->label == POST_SCROLL;
  if (effectively_scrolling && extrapolated_touch_ < kMaxNumOfExtrapolations &&
      (touch_info->touch_point.timestamp == last_touch_timestamp_ ||
       state_->cur_touch_point.position == state_->prev_touch_point.position)) {
    extrapolated_touch_++;
    touch_position_changed_ = true;
    // Fill the touch_info
    float duration = (current_timestamp - last_timestamp_).InSecondsF();
    touch_info->touch_point.position.set_x(
        state_->cur_touch_point.position.x() +
        state_->overall_velocity.x() * duration);
    touch_info->touch_point.position.set_y(
        state_->cur_touch_point.position.y() +
        state_->overall_velocity.y() * duration);
  } else {
    if (extrapolated_touch_ == kMaxNumOfExtrapolations) {
      state_->overall_velocity = {0, 0};
    }
    extrapolated_touch_ = 0;
  }
  last_touch_timestamp_ = touch_info->touch_point.timestamp;
  last_timestamp_ = current_timestamp;
}

void GestureDetector::UpdateOverallVelocity(const TouchInfo& touch_info) {
  float duration =
      (touch_info.touch_point.timestamp - state_->prev_touch_point.timestamp)
          .InSecondsF();
  // If the timestamp does not change, do not update velocity.
  if (duration < kDelta)
    return;

  const gfx::Vector2dF& displacement =
      touch_info.touch_point.position - state_->prev_touch_point.position;

  const gfx::Vector2dF& velocity = ScaleVector2d(displacement, (1 / duration));

  float weight = duration / (kRC + duration);

  state_->overall_velocity =
      ScaleVector2d(state_->overall_velocity, (1 - weight)) +
      ScaleVector2d(velocity, weight);
}

void GestureDetector::UpdateGestureParameters(const TouchInfo& touch_info) {
  state_->displacement =
      touch_info.touch_point.position - state_->prev_touch_point.position;
}

bool GestureDetector::InSlop(const gfx::Vector2dF touch_position) const {
  return (std::abs(touch_position.x() -
                   state_->initial_touch_point.position.x()) <
          kSlopHorizontal) &&
         (std::abs(touch_position.y() -
                   state_->initial_touch_point.position.y()) < kSlopVertical);
}

void GestureDetector::Reset() {
  state_ = std::make_unique<GestureDetectorState>();
}

}  // namespace vr
