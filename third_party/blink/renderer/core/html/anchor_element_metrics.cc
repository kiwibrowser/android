// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/histogram.h"

namespace blink {

IntSize AnchorElementMetrics::AccumulatedScrollOffset(
    const HTMLAnchorElement& anchor_element) {
  IntSize offset;
  Frame* frame = anchor_element.GetDocument().GetFrame();
  while (frame && frame->View() && frame->IsLocalFrame()) {
    offset += ToLocalFrame(frame)->View()->LayoutViewport()->ScrollOffsetInt();
    frame = frame->Tree().Parent();
  }
  return offset;
}

bool AnchorElementMetrics::IsInIFrame(const HTMLAnchorElement& anchor_element) {
  Frame* frame = anchor_element.GetDocument().GetFrame();
  while (frame && frame->IsLocalFrame()) {
    HTMLFrameOwnerElement* owner =
        ToLocalFrame(frame)->GetDocument()->LocalOwner();
    if (owner && IsHTMLIFrameElement(owner))
      return true;
    frame = frame->Tree().Parent();
  }
  return false;
}

base::Optional<AnchorElementMetrics> AnchorElementMetrics::From(
    const HTMLAnchorElement& anchor_element) {
  LocalFrame* local_frame = anchor_element.GetDocument().GetFrame();
  LayoutObject* layout_object = anchor_element.GetLayoutObject();
  if (!local_frame || !layout_object)
    return base::nullopt;

  LocalFrameView* local_frame_view = local_frame->View();
  LocalFrameView* root_frame_view = local_frame->LocalFrameRoot().View();
  if (!local_frame_view || !root_frame_view)
    return base::nullopt;

  IntSize visible_size =
      root_frame_view->LayoutViewport()->VisibleContentRect().Size();
  if (visible_size.IsEmpty())
    return base::nullopt;

  IntRect target_rect = EnclosingIntRect(layout_object->AbsoluteVisualRect());

  // Adjust target location due to root layer scrolling.
  // Then map the target location to the root frame.
  IntPoint target_location(target_rect.Location());
  target_location.Move(-local_frame_view->LayoutViewport()->ScrollOffsetInt());
  target_location = local_frame_view->ConvertToRootFrame(target_location);

  // Calculate features of the anchor element.
  float ratio_area = FloatSize(target_rect.Size()).Area() / visible_size.Area();
  float ratio_distance_root_top =
      float(target_location.Y() +
            AccumulatedScrollOffset(anchor_element).Height()) /
      visible_size.Height();
  float ratio_distance_visible_top =
      float(target_location.Y()) / visible_size.Height();

  return AnchorElementMetrics(ratio_area, ratio_distance_root_top,
                              ratio_distance_visible_top,
                              IsInIFrame(anchor_element));
}

void AnchorElementMetrics::RecordMetrics() const {
  UMA_HISTOGRAM_PERCENTAGE("AnchorElementMetrics.Clicked.RatioArea",
                           int(ratio_area * 100));

  UMA_HISTOGRAM_COUNTS_10000(
      "AnchorElementMetrics.Clicked.RatioDistanceRootTop",
      int(ratio_distance_root_top * 100));

  UMA_HISTOGRAM_PERCENTAGE(
      "AnchorElementMetrics.Clicked.RatioDistanceVisibleTop",
      int(ratio_distance_visible_top * 100));

  UMA_HISTOGRAM_BOOLEAN("AnchorElementMetrics.Clicked.IsInIFrame",
                        is_in_iframe);
}

}  // namespace blink
