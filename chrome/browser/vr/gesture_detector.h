// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_GESTURE_DETECTOR_H_
#define CHROME_BROWSER_VR_GESTURE_DETECTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/vr_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebGestureEvent;
}

namespace vr {

using GestureList = std::vector<std::unique_ptr<blink::WebGestureEvent>>;

struct TouchPoint {
  gfx::Vector2dF position;
  base::TimeTicks timestamp;
};

struct TouchInfo {
  TouchPoint touch_point;
  bool touch_up;
  bool touch_down;
  bool is_touching;
};

class VR_EXPORT GestureDetector {
 public:
  GestureDetector();
  virtual ~GestureDetector();

  std::unique_ptr<GestureList> DetectGestures(const TouchInfo& touch_info,
                                              base::TimeTicks current_timestamp,
                                              bool force_cancel);

 private:
  enum GestureDetectorStateLabel {
    WAITING,     // waiting for user to touch down
    TOUCHING,    // touching the touch pad but not scrolling
    SCROLLING,   // scrolling on the touch pad
    POST_SCROLL  // scroll has finished and we are hallucinating events
  };

  struct GestureDetectorState {
    GestureDetectorStateLabel label = WAITING;
    TouchPoint prev_touch_point;
    TouchPoint cur_touch_point;
    TouchPoint initial_touch_point;
    gfx::Vector2dF overall_velocity;

    // Displacement of the touch point from the previews to the current touch
    gfx::Vector2dF displacement;
  };

  std::unique_ptr<blink::WebGestureEvent> GetGestureFromTouchInfo(
      const TouchInfo& input_touch_info,
      bool force_cancel);

  void HandleWaitingState(const TouchInfo& touch_info,
                          blink::WebGestureEvent* gesture);
  void HandleDetectingState(const TouchInfo& touch_info,
                            bool force_cancel,
                            blink::WebGestureEvent* gesture);
  void HandleScrollingState(const TouchInfo& touch_info,
                            bool force_cancel,
                            blink::WebGestureEvent* gesture);
  void HandlePostScrollingState(const TouchInfo& touch_info,
                                bool force_cancel,
                                blink::WebGestureEvent* gesture);

  void UpdateGestureWithScrollDelta(blink::WebGestureEvent* gesture);

  // If the user is touching the touch pad and the touch point is different from
  // before, update the touch point and return true. Otherwise, return false.
  bool UpdateCurrentTouchPoint(const TouchInfo& touch_info);

  void ExtrapolateTouchInfo(TouchInfo* touch_info,
                            base::TimeTicks current_timestamp);

  void UpdateOverallVelocity(const TouchInfo& touch_info);

  void UpdateGestureParameters(const TouchInfo& touch_info);

  bool InSlop(const gfx::Vector2dF touch_position) const;

  void Reset();

  std::unique_ptr<GestureDetectorState> state_;

  // Number of consecutively extrapolated touch points
  int extrapolated_touch_ = 0;

  base::TimeTicks last_touch_timestamp_;
  base::TimeTicks last_timestamp_;

  bool touch_position_changed_;

  DISALLOW_COPY_AND_ASSIGN(GestureDetector);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_GESTURE_DETECTOR_H_
