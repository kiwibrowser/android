// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_GESTURE_DISPATCHER_H_
#define CHROMECAST_BROWSER_CAST_GESTURE_DISPATCHER_H_

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/graphics/cast_gesture_handler.h"

namespace chromecast {

namespace shell {

// Receives root window level gestures, interprets them, and dispatches them to
// the CastContentWindow::Delegate.
class CastGestureDispatcher : public CastGestureHandler {
 public:
  explicit CastGestureDispatcher(CastContentWindow::Delegate* delegate);

  // CastGestureHandler implementation:
  bool CanHandleSwipe(CastSideSwipeOrigin swipe_origin) override;
  void HandleSideSwipeBegin(CastSideSwipeOrigin swipe_origin,
                            const gfx::Point& touch_location) override;
  void HandleSideSwipeContinue(CastSideSwipeOrigin swipe_origin,
                               const gfx::Point& touch_location) override;
  void HandleSideSwipeEnd(CastSideSwipeOrigin swipe_origin,
                          const gfx::Point& touch_location) override;
  void HandleTapGesture(const gfx::Point& touch_location) override;

 private:
  // Number of pixels past swipe origin to consider as a back gesture.
  const int horizontal_threshold_;
  CastContentWindow::Delegate* const delegate_;
  bool dispatched_back_;
  base::ElapsedTimer current_swipe_time_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_GESTURE_DISPATCHER_H_
