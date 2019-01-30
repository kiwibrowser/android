// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/low_memory_mode_controller.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8.h"

namespace content {

LowMemoryModeController::LowMemoryModeController() = default;

LowMemoryModeController::~LowMemoryModeController() = default;

void LowMemoryModeController::OnFrameCreated(bool is_main_frame) {
  if (is_main_frame) {
    // If the process is gaining its first main frame, disable memory
    // savings mode.
    if (++main_frame_count_ == 1) {
      Disable();
    }
  } else if (main_frame_count_ == 0) {
    // The process is getting a new frame and none is main, enable
    // memory savings mode (if not already on).
    Enable();
  }
}

void LowMemoryModeController::OnFrameDestroyed(bool is_main_frame) {
  // If the process is losing its last main frame, enable memory
  // savings mode.
  if (is_main_frame && --main_frame_count_ == 0) {
    Enable();
  }
}

void LowMemoryModeController::Enable() {
  if (is_enabled_)
    return;

  blink::MainThreadIsolate()->EnableMemorySavingsMode();
  RecordHistogram(true);
  is_enabled_ = true;
}

void LowMemoryModeController::Disable() {
  if (!is_enabled_)
    return;

  blink::MainThreadIsolate()->DisableMemorySavingsMode();
  RecordHistogram(false);
  is_enabled_ = false;
}

void LowMemoryModeController::RecordHistogram(bool enabled) {
  UMA_HISTOGRAM_BOOLEAN("SiteIsolation.LowMemoryMode.Transition", enabled);
}

}  // namespace content
