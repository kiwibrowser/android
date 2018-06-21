// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_metadata_util.h"

#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace {

// Used to accomodate finite precision when comparing scaled viewport and
// content widths. While this value may seem large, width=device-width on an N7
// V1 saw errors of ~0.065 between computed window and content widths.
const float kMobileViewportWidthEpsilon = 0.15f;

bool HasFixedPageScale(float min_page_scale_factor,
                       float max_page_scale_factor) {
  return min_page_scale_factor == max_page_scale_factor;
}

bool HasMobileViewport(float page_scale_factor,
                       const gfx::SizeF& scrollable_viewport_size,
                       const gfx::SizeF& root_layer_size) {
  float window_width_dip = page_scale_factor * scrollable_viewport_size.width();
  float content_width_css = root_layer_size.width();
  return content_width_css <= window_width_dip + kMobileViewportWidthEpsilon;
}

}  // namespace

namespace content {

bool IsMobileOptimizedFrame(float page_scale_factor,
                            float min_page_scale_factor,
                            float max_page_scale_factor,
                            const gfx::SizeF& scrollable_viewport_size,
                            const gfx::SizeF& root_layer_size) {
  bool has_mobile_viewport = HasMobileViewport(
      page_scale_factor, scrollable_viewport_size, root_layer_size);
  bool has_fixed_page_scale =
      HasFixedPageScale(min_page_scale_factor, max_page_scale_factor);
  return has_fixed_page_scale || has_mobile_viewport;
}

}  // namespace content
