// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/scroll_predictor.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/least_squares_predictor.h"

using blink::WebInputEvent;
using blink::WebGestureEvent;

namespace ui {

namespace {

constexpr char kPredictor[] = "predictor";
constexpr char kScrollPredictorTypeLsq[] = "lsq";

}  // namespace

ScrollPredictor::ScrollPredictor() {
  std::string predictor_type_ = GetFieldTrialParamValueByFeature(
      features::kResamplingScrollEvents, kPredictor);
  if (predictor_type_ == kScrollPredictorTypeLsq)
    predictor_ = std::make_unique<LeastSquaresPredictor>();
  else
    predictor_ = std::make_unique<EmptyPredictor>();
}

ScrollPredictor::~ScrollPredictor() = default;

void ScrollPredictor::HandleEvent(
    const EventWithCallback::OriginalEventList& original_events,
    base::TimeTicks frame_time,
    WebInputEvent* event) {
  if (event->GetType() == WebInputEvent::kGestureScrollBegin) {
    predictor_->Reset();
    current_accumulated_delta_ = gfx::PointF();
    last_accumulated_delta_ = gfx::PointF();
  } else if (event->GetType() == WebInputEvent::kGestureScrollUpdate) {
    // TODO(eirage): When scroll events are coalesced with pinch, we can have
    // empty original event list. In that case, we can't use the original events
    // to update the prediction. We don't want to use the aggregated event to
    // update because of the event time stamp, so skip the prediction for now.
    if (original_events.empty())
      return;

    for (auto& coalesced_event : original_events)
      UpdatePrediction(coalesced_event.event_);
    ResampleEvent(frame_time, event);
  }
}

void ScrollPredictor::UpdatePrediction(const WebScopedInputEvent& event) {
  DCHECK(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(*event);
  current_accumulated_delta_.Offset(gesture_event.data.scroll_update.delta_x,
                                    gesture_event.data.scroll_update.delta_y);
  InputPredictor::InputData data = {current_accumulated_delta_,
                                    gesture_event.TimeStamp()};
  predictor_->Update(data);
}

void ScrollPredictor::ResampleEvent(base::TimeTicks time_stamp,
                                    WebInputEvent* event) {
  DCHECK(event->GetType() == WebInputEvent::kGestureScrollUpdate);
  WebGestureEvent* gesture_event = static_cast<WebGestureEvent*>(event);

  InputPredictor::InputData result;
  if (predictor_->HasPrediction() &&
      predictor_->GeneratePrediction(time_stamp, &result)) {
    gfx::PointF predicted_accumulated_delta_ = result.pos;
    gesture_event->data.scroll_update.delta_x =
        predicted_accumulated_delta_.x() - last_accumulated_delta_.x();
    gesture_event->data.scroll_update.delta_y =
        predicted_accumulated_delta_.y() - last_accumulated_delta_.y();
    gesture_event->SetTimeStamp(time_stamp);
    last_accumulated_delta_ = predicted_accumulated_delta_;
  } else {
    last_accumulated_delta_.Offset(gesture_event->data.scroll_update.delta_x,
                                   gesture_event->data.scroll_update.delta_y);
  }
}

}  // namespace ui
