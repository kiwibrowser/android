// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/false_touch_finder.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/touch_filter/edge_touch_filter.h"
#include "ui/events/ozone/evdev/touch_filter/far_apart_taps_touch_noise_filter.h"
#include "ui/events/ozone/evdev/touch_filter/horizontally_aligned_touch_noise_filter.h"
#include "ui/events/ozone/evdev/touch_filter/single_position_touch_noise_filter.h"
#include "ui/events/ozone/evdev/touch_filter/touch_filter.h"

namespace ui {

FalseTouchFinder::~FalseTouchFinder() {}

std::unique_ptr<FalseTouchFinder> FalseTouchFinder::Create(
    gfx::Size touchscreen_size) {
  bool noise_filtering = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExtraTouchNoiseFiltering);
  bool edge_filtering = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEdgeTouchFiltering);
  if (noise_filtering || edge_filtering) {
    return base::WrapUnique(new FalseTouchFinder(
        noise_filtering, edge_filtering, touchscreen_size));
  }
  return nullptr;
}

void FalseTouchFinder::HandleTouches(
    const std::vector<InProgressTouchEvdev>& touches,
    base::TimeTicks time) {
  for (const InProgressTouchEvdev& touch : touches) {
    slots_should_delay_.set(touch.slot, false);
    if (!touch.was_touching)
      slots_with_noise_.set(touch.slot, false);
  }

  bool had_noise = slots_with_noise_.any();

  for (const auto& filter : noise_filters_)
    filter->Filter(touches, time, &slots_with_noise_);

  for (const auto& filter : delay_filters_)
    filter->Filter(touches, time, &slots_should_delay_);

  RecordUMA(had_noise, time);
}

bool FalseTouchFinder::SlotHasNoise(size_t slot) const {
  return slots_with_noise_.test(slot);
}

bool FalseTouchFinder::SlotShouldDelay(size_t slot) const {
  return slots_should_delay_.test(slot);
}

FalseTouchFinder::FalseTouchFinder(bool noise_filtering,
                                   bool edge_filtering,
                                   gfx::Size touchscreen_size)
    : last_noise_time_(ui::EventTimeForNow()) {
  if (noise_filtering) {
    noise_filters_.push_back(std::make_unique<FarApartTapsTouchNoiseFilter>());
    noise_filters_.push_back(
        std::make_unique<HorizontallyAlignedTouchNoiseFilter>());
    noise_filters_.push_back(
        std::make_unique<SinglePositionTouchNoiseFilter>());
  }
  if (edge_filtering) {
    delay_filters_.push_back(
        std::make_unique<EdgeTouchFilter>(touchscreen_size));
  }
}

void FalseTouchFinder::RecordUMA(bool had_noise, base::TimeTicks time) {
  if (slots_with_noise_.any()) {
    if (!had_noise) {
      UMA_HISTOGRAM_LONG_TIMES(
          "Ozone.TouchNoiseFilter.TimeSinceLastNoiseOccurrence",
          time - last_noise_time_);
    }

    last_noise_time_ = time;
  }
}

}  // namespace ui
