// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/render_frame_metadata.h"

#include "build/build_config.h"

namespace cc {

RenderFrameMetadata::RenderFrameMetadata() = default;

RenderFrameMetadata::RenderFrameMetadata(const RenderFrameMetadata& other) =
    default;

RenderFrameMetadata::RenderFrameMetadata(RenderFrameMetadata&& other) = default;

RenderFrameMetadata::~RenderFrameMetadata() {}

// static
bool RenderFrameMetadata::HasAlwaysUpdateMetadataChanged(
    const RenderFrameMetadata& rfm1,
    const RenderFrameMetadata& rfm2) {
  return rfm1.root_background_color != rfm2.root_background_color ||
         rfm1.is_scroll_offset_at_top != rfm2.is_scroll_offset_at_top ||
         rfm1.selection != rfm2.selection ||
         rfm1.page_scale_factor != rfm2.page_scale_factor ||
#if defined(OS_ANDROID)
         rfm1.top_controls_height != rfm2.top_controls_height ||
         rfm1.top_controls_shown_ratio != rfm2.top_controls_shown_ratio ||
         rfm1.bottom_controls_height != rfm2.bottom_controls_height ||
         rfm1.bottom_controls_shown_ratio != rfm2.bottom_controls_shown_ratio ||
         rfm1.root_scroll_offset != rfm2.root_scroll_offset ||
         rfm1.min_page_scale_factor != rfm2.min_page_scale_factor ||
         rfm1.max_page_scale_factor != rfm2.max_page_scale_factor ||
         rfm1.root_overflow_y_hidden != rfm2.root_overflow_y_hidden ||
         rfm1.scrollable_viewport_size != rfm2.scrollable_viewport_size ||
         rfm1.root_layer_size != rfm2.root_layer_size ||
         rfm1.has_transparent_background != rfm2.has_transparent_background ||
#endif
         rfm1.is_mobile_optimized != rfm2.is_mobile_optimized ||
         rfm1.device_scale_factor != rfm2.device_scale_factor ||
         rfm1.viewport_size_in_pixels != rfm2.viewport_size_in_pixels ||
         rfm1.local_surface_id != rfm2.local_surface_id;
}

RenderFrameMetadata& RenderFrameMetadata::operator=(
    const RenderFrameMetadata&) = default;

RenderFrameMetadata& RenderFrameMetadata::operator=(
    RenderFrameMetadata&& other) = default;

bool RenderFrameMetadata::operator==(const RenderFrameMetadata& other) const {
  return root_scroll_offset == other.root_scroll_offset &&
         root_background_color == other.root_background_color &&
         is_scroll_offset_at_top == other.is_scroll_offset_at_top &&
         selection == other.selection &&
         is_mobile_optimized == other.is_mobile_optimized &&
         device_scale_factor == other.device_scale_factor &&
         viewport_size_in_pixels == other.viewport_size_in_pixels &&
         page_scale_factor == other.page_scale_factor &&
#if defined(OS_ANDROID)
         top_controls_height == other.top_controls_height &&
         top_controls_shown_ratio == other.top_controls_shown_ratio &&
         bottom_controls_height == other.bottom_controls_height &&
         bottom_controls_shown_ratio == other.bottom_controls_shown_ratio &&
         min_page_scale_factor == other.min_page_scale_factor &&
         max_page_scale_factor == other.max_page_scale_factor &&
         root_overflow_y_hidden == other.root_overflow_y_hidden &&
         scrollable_viewport_size == other.scrollable_viewport_size &&
         root_layer_size == other.root_layer_size &&
         has_transparent_background == other.has_transparent_background &&
#endif
         local_surface_id == other.local_surface_id;
}

bool RenderFrameMetadata::operator!=(const RenderFrameMetadata& other) const {
  return !operator==(other);
}

}  // namespace cc
