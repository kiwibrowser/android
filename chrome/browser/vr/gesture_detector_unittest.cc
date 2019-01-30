// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/gesture_detector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_gesture_event.h"

namespace {
constexpr float kDelta = 0.001f;
}

namespace vr {

TEST(GestureDetector, NotTouching) {
  GestureDetector detector;

  TouchInfo touch_info{
      .touch_up = false, .touch_down = false, .is_touching = false};
  auto gestures = detector.DetectGestures(touch_info, base::TimeTicks(), false);
  EXPECT_TRUE(gestures->empty());
}

TEST(GestureDetector, StartTouchWithoutMoving) {
  GestureDetector detector;

  base::TimeTicks timestamp;

  TouchInfo touch_info{
      .touch_point = {.position = {0, 0}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = true,
      .is_touching = true,
  };
  auto gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureFlingCancel);

  // A small move doesn't trigger scrolling yet.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info = TouchInfo{
      .touch_point = {.position = {kDelta, kDelta}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = true,
      .is_touching = true,
  };
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_TRUE(gestures->empty());
}

TEST(GestureDetector, StartTouchMoveAndRelease) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  TouchInfo touch_info{
      .touch_point = {.position = {0.0f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = true,
      .is_touching = true,
  };
  detector.DetectGestures(touch_info, timestamp, false);

  // Move to the right.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info = TouchInfo{
      .touch_point = {.position = {0.3f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = false,
      .is_touching = true,
  };
  auto gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollBegin);
  auto* gesture = gestures->front().get();
  EXPECT_GT(gesture->data.scroll_update.delta_x, 0.0f);
  EXPECT_EQ(gesture->data.scroll_update.delta_y, 0.0f);

  // Move slightly up.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info = TouchInfo{
      .touch_point = {.position = {0.3f, 0.01f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = false,
      .is_touching = true,
  };
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollUpdate);
  gesture = gestures->front().get();
  EXPECT_EQ(gesture->data.scroll_update.delta_x, 0.0f);
  EXPECT_GT(gesture->data.scroll_update.delta_y, 0.0f);

  // Release touch. Scroll is extrapolated for 2 frames.
  touch_info.touch_up = true;
  touch_info.is_touching = false;
  timestamp += base::TimeDelta::FromMilliseconds(1);
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollUpdate);
  gesture = gestures->front().get();
  EXPECT_GT(gesture->data.scroll_update.delta_x, 0.0f);
  EXPECT_GT(gesture->data.scroll_update.delta_y, 0.0f);
  touch_info.touch_up = false;
  timestamp += base::TimeDelta::FromMilliseconds(1);
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollUpdate);
  timestamp += base::TimeDelta::FromMilliseconds(1);
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollEnd);
}

TEST(GestureDetector, CancelDuringScrolling) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  TouchInfo touch_info{
      .touch_point = {.position = {0.0f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = true,
      .is_touching = true,
  };
  detector.DetectGestures(touch_info, timestamp, false);

  // Move to the right.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info = TouchInfo{
      .touch_point = {.position = {0.3f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = false,
      .is_touching = true,
  };
  auto gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollBegin);

  // Cancel.
  gestures = detector.DetectGestures(touch_info, timestamp, true);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollEnd);
}

TEST(GestureDetector, CancelDuringPostScrolling) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  TouchInfo touch_info{
      .touch_point = {.position = {0.0f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = true,
      .is_touching = true,
  };
  detector.DetectGestures(touch_info, timestamp, false);

  // Move to the right.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info = TouchInfo{
      .touch_point = {.position = {0.3f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = false,
      .is_touching = true,
  };
  auto gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollBegin);

  // Release touch. We should see extrapolated scrolling.
  touch_info.touch_up = true;
  touch_info.is_touching = false;
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollUpdate);

  // Cancel.
  touch_info.touch_up = false;
  gestures = detector.DetectGestures(touch_info, timestamp, true);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollEnd);
}

TEST(GestureDetector, CancelAndTouchDuringPostScrolling) {
  GestureDetector detector;
  base::TimeTicks timestamp;

  TouchInfo touch_info{
      .touch_point = {.position = {0.0f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = true,
      .is_touching = true,
  };
  detector.DetectGestures(touch_info, timestamp, false);

  // Move to the right.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info = TouchInfo{
      .touch_point = {.position = {0.3f, 0.0f}, .timestamp = timestamp},
      .touch_up = false,
      .touch_down = false,
      .is_touching = true,
  };
  auto gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollBegin);

  // Release touch. We should see extrapolated scrolling.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info.touch_up = true;
  touch_info.is_touching = false;
  gestures = detector.DetectGestures(touch_info, timestamp, false);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollUpdate);

  // Cancel and touch.
  timestamp += base::TimeDelta::FromMilliseconds(1);
  touch_info.touch_up = false;
  touch_info.touch_down = true;
  touch_info.is_touching = true;
  gestures = detector.DetectGestures(touch_info, timestamp, true);
  EXPECT_EQ(gestures->front()->GetType(),
            blink::WebInputEvent::kGestureScrollEnd);
}

}  // namespace vr
