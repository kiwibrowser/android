// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_timeline.h"

#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

ScrollTimeline::ScrollTimeline(base::Optional<ElementId> scroller_id,
                               ScrollDirection orientation,
                               double time_range)
    : active_id_(scroller_id),
      pending_id_(scroller_id),
      orientation_(orientation),
      time_range_(time_range) {}

ScrollTimeline::ScrollTimeline(base::Optional<ElementId> active_id,
                               base::Optional<ElementId> pending_id,
                               ScrollDirection orientation,
                               double time_range)
    : active_id_(active_id),
      pending_id_(pending_id),
      orientation_(orientation),
      time_range_(time_range) {}

ScrollTimeline::~ScrollTimeline() {}

std::unique_ptr<ScrollTimeline> ScrollTimeline::CreateImplInstance() const {
  return std::make_unique<ScrollTimeline>(active_id_, pending_id_, orientation_,
                                          time_range_);
}

double ScrollTimeline::CurrentTime(const ScrollTree& scroll_tree,
                                   bool is_active_tree) const {
  // If the scroller isn't in the ScrollTree, the element either no longer
  // exists or is not currently scrollable. By the spec, return an unresolved
  // time value.
  if ((is_active_tree && !active_id_) || (!is_active_tree && !pending_id_))
    return std::numeric_limits<double>::quiet_NaN();

  ElementId scroller_id =
      is_active_tree ? active_id_.value() : pending_id_.value();
  DCHECK(scroll_tree.FindNodeFromElementId(scroller_id));

  gfx::ScrollOffset offset = scroll_tree.current_scroll_offset(scroller_id);
  DCHECK_GE(offset.x(), 0);
  DCHECK_GE(offset.y(), 0);

  gfx::ScrollOffset scroll_dimensions = scroll_tree.MaxScrollOffset(
      scroll_tree.FindNodeFromElementId(scroller_id)->id);

  double current_offset = (orientation_ == Vertical) ? offset.y() : offset.x();
  double max_offset = (orientation_ == Vertical) ? scroll_dimensions.y()
                                                 : scroll_dimensions.x();

  // 3. If current scroll offset is less than startScrollOffset, return an
  // unresolved time value if fill is none or forwards, or 0 otherwise.
  // TODO(smcgruer): Implement |startScrollOffset| and |fill|.

  // 4. If current scroll offset is greater than or equal to endScrollOffset,
  // return an unresolved time value if fill is none or backwards, or the
  // effective time range otherwise.
  // TODO(smcgruer): Implement |endScrollOffset| and |fill|.

  // 5. Return the result of evaluating the following expression:
  //   ((current scroll offset - startScrollOffset) /
  //      (endScrollOffset - startScrollOffset)) * effective time range

  return (std::abs(current_offset) / max_offset) * time_range_;
}

void ScrollTimeline::PushPropertiesTo(ScrollTimeline* impl_timeline) {
  DCHECK(impl_timeline);
  impl_timeline->active_id_ = active_id_;
  impl_timeline->pending_id_ = pending_id_;
}

void ScrollTimeline::PromoteScrollTimelinePendingToActive() {
  active_id_ = pending_id_;
}

void ScrollTimeline::SetScrollerId(base::Optional<ElementId> pending_id) {
  // When the scroller id changes it will first be modified in the pending tree.
  // Then later (when the pending tree is promoted to active)
  // |PromoteScrollTimelinePendingToActive| will be called and will set the
  // |active_id_|.
  pending_id_ = pending_id;
}

}  // namespace cc
