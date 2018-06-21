// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/scroll_predictor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/least_squares_predictor.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace ui {
namespace test {

class ScrollPredictorTest : public testing::Test {
 public:
  ScrollPredictorTest() {}

  void SetUp() override {
    original_events_.clear();
    scroll_predictor_ = std::make_unique<ScrollPredictor>();
    scroll_predictor_->predictor_ = std::make_unique<EmptyPredictor>();
  }

  void SetUpLSQPredictor() {
    scroll_predictor_->predictor_ = std::make_unique<LeastSquaresPredictor>();
  }

  bool GetPrediction(ui::InputPredictor::InputData* result) const {
    return scroll_predictor_->predictor_->GeneratePrediction(
        WebInputEvent::GetStaticTimeStampForTests(), result);
  }

  WebScopedInputEvent CreateGestureScrollEvent(
      WebInputEvent::Type type,
      float delta_x = 0,
      float delta_y = 0,
      double time_delta_in_milliseconds = 0,
      blink::WebGestureDevice source_device =
          blink::kWebGestureDeviceTouchscreen) {
    WebGestureEvent gesture(
        type, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests() +
            base::TimeDelta::FromMillisecondsD(time_delta_in_milliseconds),
        source_device);
    if (type == WebInputEvent::kGestureScrollUpdate) {
      gesture.data.scroll_update.delta_x = delta_x;
      gesture.data.scroll_update.delta_y = delta_y;
    }

    original_events_.emplace_back(WebInputEventTraits::Clone(gesture),
                                  LatencyInfo(), base::NullCallback());

    return WebInputEventTraits::Clone(gesture);
  }

  void CoalesceWith(const WebScopedInputEvent& new_event,
                    WebScopedInputEvent& old_event) {
    Coalesce(*new_event, old_event.get());
  }

  void HandleResampleScrollEvents(WebScopedInputEvent& event,
                                  double time_delta_in_milliseconds = 0) {
    scroll_predictor_->HandleEvent(
        original_events_,
        WebInputEvent::GetStaticTimeStampForTests() +
            base::TimeDelta::FromMillisecondsD(time_delta_in_milliseconds),
        event.get());
    original_events_.clear();
  }

  bool PredictionAvailable(ui::InputPredictor::InputData* result,
                           double time_delta_in_milliseconds = 0) {
    return scroll_predictor_->predictor_->GeneratePrediction(
        WebInputEvent::GetStaticTimeStampForTests() +
            base::TimeDelta::FromMillisecondsD(time_delta_in_milliseconds),
        result);
  }

 protected:
  EventWithCallback::OriginalEventList original_events_;
  std::unique_ptr<ScrollPredictor> scroll_predictor_;

  DISALLOW_COPY_AND_ASSIGN(ScrollPredictorTest);
};

TEST_F(ScrollPredictorTest, ResampleGestureScrollEvents) {
  WebScopedInputEvent gesture_begin =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollBegin);
  HandleResampleScrollEvents(gesture_begin);

  ui::InputPredictor::InputData result;
  EXPECT_FALSE(PredictionAvailable(&result));

  WebScopedInputEvent gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -20);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Aggregated event delta doesn't change with empty predictor applied.
  gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -20);
  CoalesceWith(
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -40),
      gesture_update);
  EXPECT_EQ(-60,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-60,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Cumulative amount of scroll from the GSB is stored in the empty predictor.
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-80, result.pos.y());

  // Send another GSB, Prediction will be reset.
  gesture_begin = CreateGestureScrollEvent(WebInputEvent::kGestureScrollBegin);
  HandleResampleScrollEvents(gesture_begin);
  EXPECT_FALSE(PredictionAvailable(&result));

  // Sent another GSU.
  gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -35);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-35,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  // Total amount of scroll is track from the last GSB.
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-35, result.pos.y());
}

TEST_F(ScrollPredictorTest, ScrollInDifferentDirection) {
  WebScopedInputEvent gesture_begin =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollBegin);
  HandleResampleScrollEvents(gesture_begin);

  ui::InputPredictor::InputData result;

  // Scroll down.
  WebScopedInputEvent gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -20);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-20, result.pos.y());

  // Scroll up.
  gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, 25);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(0, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_x);
  EXPECT_EQ(25, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_y);
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(0, result.pos.x());
  EXPECT_EQ(5, result.pos.y());

  // Scroll left + right.
  gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, -35, 0);
  CoalesceWith(
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 60, 0),
      gesture_update);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(25, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                    ->data.scroll_update.delta_x);
  EXPECT_EQ(0, static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                   ->data.scroll_update.delta_y);
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(25, result.pos.x());
  EXPECT_EQ(5, result.pos.y());
}

TEST_F(ScrollPredictorTest, ScrollUpdateWithEmptyOriginalEventList) {
  WebScopedInputEvent gesture_begin =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollBegin);
  HandleResampleScrollEvents(gesture_begin);

  // Send a GSU with empty original event list.
  WebScopedInputEvent gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -20);
  original_events_.clear();
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-20,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // No prediction available because the event is skipped.
  ui::InputPredictor::InputData result;
  EXPECT_FALSE(PredictionAvailable(&result));

  // Send a GSU with original event.
  gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -30);
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Send another GSU with empty original event list.
  gesture_update =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate, 0, -40);
  original_events_.clear();
  HandleResampleScrollEvents(gesture_update);
  EXPECT_EQ(-40,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);

  // Prediction only track GSU with original event list.
  EXPECT_TRUE(PredictionAvailable(&result));
  EXPECT_EQ(-30, result.pos.y());
}

TEST_F(ScrollPredictorTest, LSQPredictorTest) {
  SetUpLSQPredictor();
  WebScopedInputEvent gesture_begin =
      CreateGestureScrollEvent(WebInputEvent::kGestureScrollBegin);
  HandleResampleScrollEvents(gesture_begin);
  ui::InputPredictor::InputData result;

  // Send 1st GSU, no prediction available.
  WebScopedInputEvent gesture_update = CreateGestureScrollEvent(
      WebInputEvent::kGestureScrollUpdate, 0, -30, 8 /* ms */);
  HandleResampleScrollEvents(gesture_update, 16 /* ms */);
  EXPECT_FALSE(PredictionAvailable(&result, 16 /* ms */));
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(8 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());

  // Send 2nd GSU, no prediction available, event aligned at original timestamp.
  gesture_update = CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate,
                                            0, -30, 16 /* ms */);
  HandleResampleScrollEvents(gesture_update, 24 /* ms */);
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(16 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());
  EXPECT_FALSE(PredictionAvailable(&result, 24 /* ms */));

  // Send 3rd and 4th GSU, prediction result returns the sum of delta_y, event
  // aligned at frame time.
  gesture_update = CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate,
                                            0, -30, 24 /* ms */);
  HandleResampleScrollEvents(gesture_update, 32 /* ms */);
  EXPECT_EQ(-60,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(32 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());
  EXPECT_TRUE(PredictionAvailable(&result, 32 /* ms */));
  EXPECT_EQ(-120, result.pos.y());

  gesture_update = CreateGestureScrollEvent(WebInputEvent::kGestureScrollUpdate,
                                            0, -30, 32 /* ms */);
  HandleResampleScrollEvents(gesture_update, 40 /* ms */);
  EXPECT_EQ(-30,
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::GetStaticTimeStampForTests() +
                base::TimeDelta::FromMillisecondsD(40 /* ms */),
            static_cast<const blink::WebGestureEvent*>(gesture_update.get())
                ->TimeStamp());
  EXPECT_TRUE(PredictionAvailable(&result, 40 /* ms */));
  EXPECT_EQ(-150, result.pos.y());
}

}  // namespace test
}  // namespace ui
