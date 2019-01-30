// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class HTMLAnchorElement;
class IntSize;

class CORE_EXPORT AnchorElementMetrics {
 public:
  // Extract features of the anchor element.
  static base::Optional<AnchorElementMetrics> From(const HTMLAnchorElement&);

  // Get and upload anchor element features.
  void RecordMetrics() const;

  float GetRatioArea() const { return ratio_area; }
  float GetRatioDistanceRootTop() const { return ratio_distance_root_top; }
  float GetRatioDistanceVisibleTop() const {
    return ratio_distance_visible_top;
  }
  bool GetIsInIframe() const { return is_in_iframe; }

 private:
  // Accumulated scroll offset of all frames up to the local root frame.
  static IntSize AccumulatedScrollOffset(const HTMLAnchorElement&);

  // Whether the element is inside an iframe.
  static bool IsInIFrame(const HTMLAnchorElement&);

  // The ratio of the clickable region area of an anchor element, and the
  // viewport area.
  const float ratio_area;

  // The distance between the top of the clickable region of an anchor element
  // and the top edge of the root frame, divided by the viewport height.
  const float ratio_distance_root_top;

  // The distance between the top of the clickable region of an anchor element
  // and the top edge of the visible region, divided by the viewport height.
  const float ratio_distance_visible_top;

  // Whether the anchor element is within an iframe.
  const bool is_in_iframe;

  inline AnchorElementMetrics(float ratio_area,
                              float ratio_distance_root_top,
                              float ratio_distance_visible_top,
                              bool is_in_iframe)
      : ratio_area(ratio_area),
        ratio_distance_root_top(ratio_distance_root_top),
        ratio_distance_visible_top(ratio_distance_visible_top),
        is_in_iframe(is_in_iframe) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_H_
