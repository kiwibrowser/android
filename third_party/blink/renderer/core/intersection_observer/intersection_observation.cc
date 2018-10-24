// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intersection_geometry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {

bool IsOccluded(const Element& element, const IntersectionGeometry& geometry) {
  HitTestResult hits(
      element.GetLayoutObject()->HitTestForOcclusion(geometry.TargetRect()));
  const HitTestResult::NodeSet& hit_nodes = hits.ListBasedTestResult();
  for (const auto& node : hit_nodes) {
    if (!node->contains(&element) &&
        node->GetLayoutObject()->HasNonZeroEffectiveOpacity()) {
      return true;
    }
  }
  return false;
}

}  // namespace

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer,
                                                 Element& target)
    : observer_(observer),
      target_(&target),
      // Note that the spec says the initial value of m_lastThresholdIndex
      // should be -1, but since m_lastThresholdIndex is unsigned, we use a
      // different sentinel value.
      last_is_visible_(false),
      last_threshold_index_(kMaxThresholdIndex - 1) {
  UpdateShouldReportRootBoundsAfterDomChange();
}

void IntersectionObservation::ComputeIntersectionObservations(
    DOMHighResTimeStamp timestamp) {
  DCHECK(Observer());
  if (!target_)
    return;
  Vector<Length> root_margin(4);
  root_margin[0] = observer_->TopMargin();
  root_margin[1] = observer_->RightMargin();
  root_margin[2] = observer_->BottomMargin();
  root_margin[3] = observer_->LeftMargin();
  IntersectionGeometry geometry(observer_->root(), *Target(), root_margin,
                                should_report_root_bounds_);
  geometry.ComputeGeometry();

  // Some corner cases for threshold index:
  //   - If target rect is zero area, because it has zero width and/or zero
  //     height,
  //     only two states are recognized:
  //     - 0 means not intersecting.
  //     - 1 means intersecting.
  //     No other threshold crossings are possible.
  //   - Otherwise:
  //     - If root and target do not intersect, the threshold index is 0.
  //     - If root and target intersect but the intersection has zero-area
  //       (i.e., they have a coincident edge or corner), we consider the
  //       intersection to have "crossed" a zero threshold, but not crossed
  //       any non-zero threshold.
  unsigned new_threshold_index;
  float new_visible_ratio;
  bool is_visible = false;
  if (geometry.DoesIntersect()) {
    if (geometry.TargetRect().IsEmpty()) {
      new_visible_ratio = 1;
    } else {
      float intersection_area =
          geometry.IntersectionRect().Size().Width().ToFloat() *
          geometry.IntersectionRect().Size().Height().ToFloat();
      float target_area = geometry.TargetRect().Size().Width().ToFloat() *
                          geometry.TargetRect().Size().Height().ToFloat();
      new_visible_ratio = intersection_area / target_area;
    }
    new_threshold_index =
        Observer()->FirstThresholdGreaterThan(new_visible_ratio);
    if (RuntimeEnabledFeatures::IntersectionObserverV2Enabled() &&
        Observer()->trackVisibility()) {
      is_visible = !Target()->GetLayoutObject()->HasDistortingVisualEffects() &&
                   !IsOccluded(*Target(), geometry);
    }
  } else {
    new_visible_ratio = 0;
    new_threshold_index = 0;
  }

  // TODO(tkent): We can't use CHECK_LT due to a compile error.
  CHECK(new_threshold_index < kMaxThresholdIndex);

  if (last_threshold_index_ != new_threshold_index ||
      last_is_visible_ != is_visible) {
    FloatRect snapped_root_bounds(geometry.RootRect());
    FloatRect* root_bounds_pointer =
        should_report_root_bounds_ ? &snapped_root_bounds : nullptr;
    IntersectionObserverEntry* new_entry = new IntersectionObserverEntry(
        timestamp, new_visible_ratio, FloatRect(geometry.TargetRect()),
        root_bounds_pointer, FloatRect(geometry.IntersectionRect()),
        geometry.DoesIntersect(), is_visible, Target());
    Observer()->EnqueueIntersectionObserverEntry(*new_entry);
    SetLastThresholdIndex(new_threshold_index);
    SetWasVisible(is_visible);
  }
}

void IntersectionObservation::Disconnect() {
  DCHECK(Observer());
  if (target_)
    Target()->EnsureIntersectionObserverData().RemoveObservation(*Observer());
  observer_.Clear();
}

void IntersectionObservation::UpdateShouldReportRootBoundsAfterDomChange() {
  if (!Observer()->RootIsImplicit()) {
    should_report_root_bounds_ = true;
    return;
  }
  should_report_root_bounds_ = false;
  LocalFrame* target_frame = Target()->GetDocument().GetFrame();
  if (!target_frame)
    return;
  Frame& root_frame = target_frame->Tree().Top();
  if (&root_frame == target_frame) {
    should_report_root_bounds_ = true;
  } else {
    should_report_root_bounds_ =
        target_frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            root_frame.GetSecurityContext()->GetSecurityOrigin());
  }
}

void IntersectionObservation::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(target_);
}

}  // namespace blink
