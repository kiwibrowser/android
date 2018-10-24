// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_break_token.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

NGPhysicalLineBoxFragment::NGPhysicalLineBoxFragment(
    const ComputedStyle& style,
    NGStyleVariant style_variant,
    NGPhysicalSize size,
    Vector<scoped_refptr<NGPhysicalFragment>>& children,
    const NGPhysicalOffsetRect& contents_ink_overflow,
    const NGPhysicalOffsetRect& scrollable_overflow,
    const NGLineHeightMetrics& metrics,
    TextDirection base_direction,
    scoped_refptr<NGBreakToken> break_token)
    : NGPhysicalContainerFragment(nullptr,
                                  style,
                                  style_variant,
                                  size,
                                  kFragmentLineBox,
                                  0,
                                  children,
                                  contents_ink_overflow,
                                  std::move(break_token)),
      scrollable_overflow_(scrollable_overflow),
      metrics_(metrics) {
  base_direction_ = static_cast<unsigned>(base_direction);
}

NGLineHeightMetrics NGPhysicalLineBoxFragment::BaselineMetrics(
    FontBaseline) const {
  // TODO(kojii): Computing other baseline types than the used one is not
  // implemented yet.
  // TODO(kojii): We might need locale/script to look up OpenType BASE table.
  return metrics_;
}

NGPhysicalOffsetRect NGPhysicalLineBoxFragment::InkOverflow() const {
  return ContentsInkOverflow();
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::FirstLogicalLeaf() const {
  if (Children().IsEmpty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (runner->IsContainer() && !runner->IsBlockLayoutRoot()) {
    const NGPhysicalContainerFragment* runner_as_container =
        ToNGPhysicalContainerFragment(runner);
    if (runner_as_container->Children().IsEmpty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().front().get()
                 : runner_as_container->Children().back().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

const NGPhysicalFragment* NGPhysicalLineBoxFragment::LastLogicalLeaf() const {
  if (Children().IsEmpty())
    return nullptr;
  // TODO(xiaochengh): This isn't correct for mixed Bidi. Fix it. Besides, we
  // should compute and store it during layout.
  const TextDirection direction = Style().Direction();
  const NGPhysicalFragment* runner = this;
  while (runner->IsContainer() && !runner->IsBlockLayoutRoot()) {
    const NGPhysicalContainerFragment* runner_as_container =
        ToNGPhysicalContainerFragment(runner);
    if (runner_as_container->Children().IsEmpty())
      break;
    runner = direction == TextDirection::kLtr
                 ? runner_as_container->Children().back().get()
                 : runner_as_container->Children().front().get();
  }
  DCHECK_NE(runner, this);
  return runner;
}

bool NGPhysicalLineBoxFragment::HasSoftWrapToNextLine() const {
  DCHECK(BreakToken());
  DCHECK(BreakToken()->IsInlineType());
  const NGInlineBreakToken& break_token = ToNGInlineBreakToken(*BreakToken());
  return !break_token.IsFinished() && !break_token.IsForcedBreak();
}

}  // namespace blink
