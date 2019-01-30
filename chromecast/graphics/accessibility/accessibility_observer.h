// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied with modifications from ash/accessibility, refactored for use in
// chromecast.

#ifndef CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H_
#define CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H_

namespace chromecast {

class AccessibilityObserver {
 public:
  virtual ~AccessibilityObserver() = default;

  // Called when any accessibility status changes.
  virtual void OnAccessibilityStatusChanged() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_GRAPHICS_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H_
