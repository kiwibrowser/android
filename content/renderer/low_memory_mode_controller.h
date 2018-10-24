// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOW_MEMORY_MODE_CONTROLLER_H_
#define CONTENT_RENDERER_LOW_MEMORY_MODE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// The LowMemoryModeController manages for a renderer process the blink
// main thread isolate's memory savings mode state. This is only enabled
// if the V8LowMemoryModeForNonMainFrames feature and --site-per-process
// are enabled.
//
// When a process only contains subframes, the memory saving mode is
// enabled. If a main frame is later created, then the mode is disabled
// for the duration of the main frame's existence.
//
// The default state after initialization is to not enable low memory mode.
class CONTENT_EXPORT LowMemoryModeController {
 public:
  LowMemoryModeController();
  ~LowMemoryModeController();

  // Notifies the controller that a frame has either been created or
  // destroyed. A transition to the memory saving mode may occur as a result.
  void OnFrameCreated(bool is_main_frame);
  void OnFrameDestroyed(bool is_main_frame);

  bool is_enabled() const { return is_enabled_; }

 private:
  // Puts the main thread isolate into memory savings mode if it is not
  // currently enabled.
  void Enable();

  // Takes the main thread isolate out of memory savings mode if it is
  // currently enabled.
  void Disable();

  // Records an UMA histogram marking an Enabled->Disabled state transition,
  // or vice versa.
  void RecordHistogram(bool enabled);

  int main_frame_count_ = 0;
  bool is_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(LowMemoryModeController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOW_MEMORY_MODE_CONTROLLER_H_
