// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_abstract_inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

namespace blink {

namespace {

NGLogicalRect ComputeLogicalRectFor(const NGPhysicalOffsetRect& physical_rect,
                                    const NGPaintFragment& paint_fragment) {
  const WritingMode writing_mode = paint_fragment.Style().GetWritingMode();
  const TextDirection text_direction =
      paint_fragment.PhysicalFragment().ResolvedDirection();
  const NGPhysicalSize outer_size = paint_fragment.Size();
  const NGLogicalOffset logical_offset = physical_rect.offset.ConvertToLogical(
      writing_mode, text_direction, outer_size, physical_rect.size);
  const NGLogicalSize logical_size =
      physical_rect.size.ConvertToLogical(writing_mode);
  return {logical_offset, logical_size};
}

NGPhysicalOffsetRect ComputePhysicalRectFor(
    const NGLogicalRect& logical_rect,
    const NGPaintFragment& paint_fragment) {
  const WritingMode writing_mode = paint_fragment.Style().GetWritingMode();
  const TextDirection text_direction =
      paint_fragment.PhysicalFragment().ResolvedDirection();
  const NGPhysicalSize outer_size = paint_fragment.Size();
  const NGPhysicalSize physical_size =
      logical_rect.size.ConvertToPhysical(writing_mode);
  const NGPhysicalOffset physical_offset =
      logical_rect.offset.ConvertToPhysical(writing_mode, text_direction,
                                            outer_size, physical_size);

  return {physical_offset, physical_size};
}

NGLogicalRect ExpandedSelectionRectForSoftLineBreakIfNeeded(
    const NGLogicalRect& rect,
    const NGPaintFragment& paint_fragment,
    const LayoutSelectionStatus& selection_status) {
  // Expand paint rect if selection covers multiple lines and
  // this fragment is at the end of line.
  if (selection_status.line_break == SelectLineBreak::kNotSelected)
    return rect;
  if (paint_fragment.GetLayoutObject()
          ->EnclosingNGBlockFlow()
          ->ShouldTruncateOverflowingText())
    return rect;
  // Copy from InlineTextBoxPainter::PaintSelection.
  const LayoutUnit space_width(paint_fragment.Style().GetFont().SpaceWidth());
  return {rect.offset,
          {rect.size.inline_size + space_width, rect.size.block_size}};
}

// Expands selection height so that the selection rect fills entire line.
NGLogicalRect ExpandSelectionRectToLineHeight(
    const NGLogicalRect& rect,
    const NGPaintFragment& paint_fragment) {
  const NGPaintFragment* current_line = paint_fragment.ContainerLineBox();
  DCHECK(current_line);
  const NGPhysicalOffsetRect line_physical_rect(
      current_line->InlineOffsetToContainerBox() -
          paint_fragment.InlineOffsetToContainerBox(),
      current_line->Size());
  const NGLogicalRect line_logical_rect =
      ComputeLogicalRectFor(line_physical_rect, paint_fragment);
  return {{rect.offset.inline_offset, line_logical_rect.offset.block_offset},
          {rect.size.inline_size, line_logical_rect.size.block_size}};
}

NGLogicalOffset ChildLogicalOffsetInParent(const NGPaintFragment& child) {
  DCHECK(child.Parent());
  const NGPaintFragment& parent = *child.Parent();
  return child.Offset().ConvertToLogical(parent.Style().GetWritingMode(),
                                         parent.Style().Direction(),
                                         parent.Size(), child.Size());
}

NGLogicalSize ChildLogicalSizeInParent(const NGPaintFragment& child) {
  DCHECK(child.Parent());
  const NGPaintFragment& parent = *child.Parent();
  return NGFragment(parent.Style().GetWritingMode(), child.PhysicalFragment())
      .Size();
}

base::Optional<PositionWithAffinity> PositionForPointInChild(
    const NGPaintFragment& child,
    const NGPhysicalOffset& point) {
  const NGPhysicalOffset& child_point = point - child.Offset();
  // We must fallback to legacy for old layout roots. We also fallback (to
  // LayoutNGMixin::PositionForPoint()) for NG block layout, so that we can
  // utilize LayoutBlock::PositionForPoint() that resolves the position in block
  // layout.
  // TODO(xiaochengh): Don't fallback to legacy for NG block layout.
  const bool should_fallback = child.PhysicalFragment().IsBlockFlow() ||
                               child.PhysicalFragment().IsOldLayoutRoot();
  const PositionWithAffinity result =
      should_fallback ? child.GetLayoutObject()->PositionForPoint(
                            child_point.ToLayoutPoint())
                      : child.PositionForPoint(child_point);
  if (result.IsNotNull())
    return result;
  return base::nullopt;
}

// ::before, ::after and ::first-letter can be hit test targets.
bool CanBeHitTestTargetPseudoNode(const Node& node) {
  if (!node.IsPseudoElement())
    return false;
  switch (ToPseudoElement(node).GetPseudoId()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdFirstLetter:
      return true;
    default:
      return false;
  }
}

bool IsLastBRInPage(const NGPhysicalTextFragment& text_fragment) {
  return text_fragment.GetLayoutObject()->IsBR() &&
         !text_fragment.GetLayoutObject()->NextInPreOrder();
}

}  // namespace

NGPaintFragment::NGPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPaintFragment* parent)
    : physical_fragment_(std::move(fragment)), parent_(parent) {
  DCHECK(physical_fragment_);
}

NGPaintFragment::~NGPaintFragment() {
  NGAbstractInlineTextBox::WillDestroy(this);
}

std::unique_ptr<NGPaintFragment> NGPaintFragment::Create(
    scoped_refptr<const NGPhysicalFragment> fragment) {
  std::unique_ptr<NGPaintFragment> paint_fragment =
      std::make_unique<NGPaintFragment>(std::move(fragment), nullptr);

  HashMap<const LayoutObject*, NGPaintFragment*> last_fragment_map;
  paint_fragment->PopulateDescendants(NGPhysicalOffset(),
                                      &paint_fragment->first_fragment_map_,
                                      &last_fragment_map);

  return paint_fragment;
}

bool NGPaintFragment::IsDescendantOfNotSelf(
    const NGPaintFragment& ancestor) const {
  for (const NGPaintFragment* fragment = Parent(); fragment;
       fragment = fragment->Parent()) {
    if (fragment == &ancestor)
      return true;
  }
  return false;
}

bool NGPaintFragment::HasSelfPaintingLayer() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).HasSelfPaintingLayer();
}

bool NGPaintFragment::HasOverflowClip() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).HasOverflowClip();
}

bool NGPaintFragment::ShouldClipOverflow() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).ShouldClipOverflow();
}

LayoutRect NGPaintFragment::SelfInkOverflow() const {
  return physical_fragment_->InkOverflow().ToLayoutRect();
}

LayoutRect NGPaintFragment::ChildrenInkOverflow() const {
  return physical_fragment_->InkOverflow(false).ToLayoutRect();
}

// Populate descendants from NGPhysicalFragment tree.
void NGPaintFragment::PopulateDescendants(
    const NGPhysicalOffset inline_offset_to_container_box,
    HashMap<const LayoutObject*, NGPaintFragment*>* first_fragment_map,
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map) {
  DCHECK(children_.IsEmpty());
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (!fragment.IsContainer())
    return;
  const NGPhysicalContainerFragment& container =
      ToNGPhysicalContainerFragment(fragment);
  children_.ReserveCapacity(container.Children().size());

  for (const auto& child_fragment : container.Children()) {
    std::unique_ptr<NGPaintFragment> child =
        std::make_unique<NGPaintFragment>(child_fragment, this);

    // Create a linked list for each LayoutObject.
    //
    // |first_fragment_map| and |last_fragment_map| each keeps the first and the
    // last of the list of fragments for a LayoutObject. We use two maps because
    // |last_fragment_map| is needed only while creating lists, while
    // |first_fragment_map| is kept for later queries. This may change when we
    // use fields in LayoutObject to keep the first fragments.
    if (LayoutObject* layout_object = child_fragment->GetLayoutObject()) {
      auto add_result = last_fragment_map->insert(layout_object, child.get());
      if (add_result.is_new_entry) {
        DCHECK(first_fragment_map->find(layout_object) ==
               first_fragment_map->end());
        first_fragment_map->insert(layout_object, child.get());
      } else {
        DCHECK(first_fragment_map->find(layout_object) !=
               first_fragment_map->end());
        DCHECK(add_result.stored_value->value);
        add_result.stored_value->value->next_fragment_ = child.get();
        add_result.stored_value->value = child.get();
      }
    }

    child->inline_offset_to_container_box_ =
        inline_offset_to_container_box + child_fragment->Offset();

    // Recurse chlidren, except when this is a block layout root.
    // TODO(kojii): At the block layout root, chlidren maybe for NGPaint,
    // LayoutNG but not for NGPaint, or legacy. In order to get the maximum
    // test coverage, split the NGPaintFragment tree at all possible engine
    // boundaries.
    if (!child_fragment->IsBlockLayoutRoot()) {
      child->PopulateDescendants(child->inline_offset_to_container_box_,
                                 first_fragment_map, last_fragment_map);
    }

    children_.push_back(std::move(child));
  }
}

NGPaintFragment* NGPaintFragment::GetForInlineContainer(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline());
  // Search from its parent because |EnclosingNGBlockFlow| returns itself when
  // the LayoutObject is a box (i.e., atomic inline, including inline block and
  // replaced elements.)
  if (LayoutObject* parent = layout_object->Parent()) {
    if (LayoutBlockFlow* block_flow = parent->EnclosingNGBlockFlow())
      return block_flow->PaintFragment();
  }
  return nullptr;
}

NGPaintFragment::FragmentRange NGPaintFragment::InlineFragmentsFor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline());
  if (const NGPaintFragment* root = GetForInlineContainer(layout_object)) {
    auto it = root->first_fragment_map_.find(layout_object);
    if (it != root->first_fragment_map_.end())
      return FragmentRange(it->value);

    // Reaching here means that there are no fragments for the LayoutObject.
    // Culled inline box is one, but can be space-only LayoutText that were
    // collapsed out.
    return FragmentRange(nullptr);
  }
  return FragmentRange(nullptr, false);
}

bool NGPaintFragment::FlippedLocalVisualRectFor(
    const LayoutObject* layout_object,
    LayoutRect* visual_rect) {
  auto fragments = InlineFragmentsFor(layout_object);
  if (!fragments.IsInLayoutNGInlineFormattingContext())
    return false;

  for (NGPaintFragment* fragment : fragments) {
    NGPhysicalOffsetRect child_visual_rect =
        fragment->PhysicalFragment().SelfInkOverflow();
    child_visual_rect.offset += fragment->InlineOffsetToContainerBox();
    visual_rect->Unite(child_visual_rect.ToLayoutRect());
  }
  if (!layout_object->HasFlippedBlocksWritingMode())
    return true;

  NGPaintFragment* container = GetForInlineContainer(layout_object);
  DCHECK(container);
  ToLayoutBox(container->GetLayoutObject())->FlipForWritingMode(*visual_rect);
  return true;
}

void NGPaintFragment::UpdateVisualRectForNonLayoutObjectChildren() {
  // Scan direct children only beause line boxes are always direct children of
  // the inline formatting context.
  for (auto& child : Children()) {
    if (!child->PhysicalFragment().IsLineBox())
      continue;
    LayoutRect union_of_children;
    for (const auto& descendant : child->Children())
      union_of_children.Unite(descendant->VisualRect());
    child->SetVisualRect(union_of_children);
  }
}

void NGPaintFragment::AddSelfOutlineRect(
    Vector<LayoutRect>* outline_rects,
    const LayoutPoint& additional_offset) const {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (fragment.IsBox()) {
    ToNGPhysicalBoxFragment(fragment).AddSelfOutlineRects(outline_rects,
                                                          additional_offset);
  }
}

void NGPaintFragment::PaintInlineBoxForDescendants(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutInline* layout_object,
    NGPhysicalOffset offset) const {
  for (const auto& child : Children()) {
    if (child->GetLayoutObject() == layout_object) {
      NGBoxFragmentPainter(*child).PaintInlineBox(
          paint_info, paint_offset + offset.ToLayoutPoint() /*, paint_offset*/);
      continue;
    }

    child->PaintInlineBoxForDescendants(paint_info, paint_offset, layout_object,
                                        offset + child->Offset());
  }
}

const NGPaintFragment* NGPaintFragment::ContainerLineBox() const {
  DCHECK(PhysicalFragment().IsInline());
  for (const NGPaintFragment* runner = this; runner;
       runner = runner->Parent()) {
    if (runner->PhysicalFragment().IsLineBox())
      return runner;
  }
  NOTREACHED();
  return nullptr;
}

NGPaintFragment* NGPaintFragment::FirstLineBox() const {
  for (auto& child : children_) {
    if (child->PhysicalFragment().IsLineBox())
      return child.get();
  }
  return nullptr;
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationRecursively() {
  if (LayoutObject* layout_object = GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();

  for (auto& child : children_)
    child->SetShouldDoFullPaintInvalidationRecursively();
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationForFirstLine() {
  DCHECK(PhysicalFragment().IsBox() && GetLayoutObject() &&
         GetLayoutObject()->IsLayoutBlockFlow());

  if (NGPaintFragment* line_box = FirstLineBox())
    line_box->SetShouldDoFullPaintInvalidationRecursively();
}

NGPhysicalOffsetRect NGPaintFragment::ComputeLocalSelectionRectForText(
    const LayoutSelectionStatus& selection_status) const {
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragmentOrDie(PhysicalFragment());
  NGPhysicalOffsetRect selection_rect =
      text_fragment.LocalRect(selection_status.start, selection_status.end);
  NGLogicalRect logical_rect = ComputeLogicalRectFor(selection_rect, *this);
  // Let LocalRect for line break have a space width to paint line break
  // when it is only character in a line or only selected in a line.
  if (text_fragment.IsLineBreak() &&
      selection_status.start != selection_status.end &&
      // This is for old compatible that old doesn't paint last br in a page.
      !IsLastBRInPage(text_fragment)) {
    DCHECK(!logical_rect.size.inline_size);
    logical_rect.size.inline_size = LayoutUnit(Style().GetFont().SpaceWidth());
  }
  const NGLogicalRect line_break_extended_rect =
      text_fragment.IsLineBreak()
          ? logical_rect
          : ExpandedSelectionRectForSoftLineBreakIfNeeded(logical_rect, *this,
                                                          selection_status);
  const NGLogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(line_break_extended_rect, *this);
  const NGPhysicalOffsetRect physical_rect =
      ComputePhysicalRectFor(line_height_expanded_rect, *this);
  return physical_rect;
}

NGPhysicalOffsetRect NGPaintFragment::ComputeLocalSelectionRectForReplaced()
    const {
  DCHECK(GetLayoutObject()->IsLayoutReplaced());
  const NGPhysicalOffsetRect selection_rect = PhysicalFragment().LocalRect();
  NGLogicalRect logical_rect = ComputeLogicalRectFor(selection_rect, *this);
  const NGLogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(logical_rect, *this);
  const NGPhysicalOffsetRect physical_rect =
      ComputePhysicalRectFor(line_height_expanded_rect, *this);
  return physical_rect;
}

PositionWithAffinity NGPaintFragment::PositionForPointInText(
    const NGPhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsText());
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(PhysicalFragment());
  if (text_fragment.IsAnonymousText())
    return PositionWithAffinity();
  const unsigned text_offset = text_fragment.TextOffsetForPoint(point);
  if (text_offset > text_fragment.StartOffset() &&
      text_offset < text_fragment.EndOffset()) {
    const Position position = NGOffsetMapping::GetFor(GetLayoutObject())
                                  ->GetFirstPosition(text_offset);
    // TODO(xiaochengh): Adjust TextAffinity.
    return PositionWithAffinity(position, TextAffinity::kDownstream);
  }
  const NGCaretPosition unadjusted_position{
      this, NGCaretPositionType::kAtTextOffset, text_offset};
  const Position adjusted_position =
      BidiAdjustment::AdjustForHitTest(unadjusted_position)
          .ToPositionInDOMTree();
  // TODO(xiaochengh): Adjust TextAffinity.
  return PositionWithAffinity(adjusted_position, TextAffinity::kDownstream);
}

PositionWithAffinity NGPaintFragment::PositionForPointInInlineLevelBox(
    const NGPhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
  DCHECK(!PhysicalFragment().IsBlockFlow());

  const NGLogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(), NGPhysicalSize());
  const LayoutUnit inline_point = logical_point.inline_offset;

  // Stores the closest child before |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_child_before = nullptr;
  LayoutUnit closest_child_before_inline_offset = LayoutUnit::Min();

  // Stores the closest child after |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_child_after = nullptr;
  LayoutUnit closest_child_after_inline_offset = LayoutUnit::Max();

  for (const auto& child : Children()) {
    const LayoutUnit child_inline_min =
        ChildLogicalOffsetInParent(*child).inline_offset;
    const LayoutUnit child_inline_max =
        child_inline_min + ChildLogicalSizeInParent(*child).inline_size;

    // Try to resolve if |point| falls in any child in inline direction.
    if (inline_point >= child_inline_min && inline_point <= child_inline_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (inline_point < child_inline_min) {
      if (child_inline_min < closest_child_after_inline_offset) {
        closest_child_after = child.get();
        closest_child_after_inline_offset = child_inline_min;
      }
    }

    if (inline_point > child_inline_max) {
      if (child_inline_max > closest_child_before_inline_offset) {
        closest_child_before = child.get();
        closest_child_before_inline_offset = child_inline_max;
      }
    }
  }

  if (closest_child_after) {
    if (auto child_position =
            PositionForPointInChild(*closest_child_after, point))
      return child_position.value();
  }

  if (closest_child_before) {
    if (auto child_position =
            PositionForPointInChild(*closest_child_before, point))
      return child_position.value();
  }

  return PositionWithAffinity();
}

PositionWithAffinity NGPaintFragment::PositionForPointInInlineFormattingContext(
    const NGPhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsBlockFlow());
  DCHECK(PhysicalFragment().IsBox());
  DCHECK(ToNGPhysicalBoxFragment(PhysicalFragment()).ChildrenInline());

  const NGLogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(), NGPhysicalSize());
  const LayoutUnit block_point = logical_point.block_offset;

  // Stores the closest line box child above |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_line_before = nullptr;
  LayoutUnit closest_line_before_block_offset = LayoutUnit::Min();

  // Stores the closest line box child below |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_line_after = nullptr;
  LayoutUnit closest_line_after_block_offset = LayoutUnit::Max();

  for (const auto& child : Children()) {
    // Try to resolve if |point| falls in a non-line-box child completely.
    if (!child->PhysicalFragment().IsLineBox()) {
      if (point.left >= child->Offset().left &&
          point.left <= child->Offset().left + child->Size().width &&
          point.top >= child->Offset().top &&
          point.top <= child->Offset().top + child->Size().height) {
        if (auto child_position = PositionForPointInChild(*child, point))
          return child_position.value();
      }
      continue;
    }

    if (!child->PhysicalFragment().IsLineBox() || child->Children().IsEmpty())
      continue;

    const LayoutUnit line_min = ChildLogicalOffsetInParent(*child).block_offset;
    const LayoutUnit line_max =
        line_min + ChildLogicalSizeInParent(*child).block_size;

    // Try to resolve if |point| falls in a line box in block direction.
    // Hitting on line bottom doesn't count, to match legacy behavior.
    // TODO(xiaochengh): Consider floats.
    if (block_point >= line_min && block_point < line_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (block_point < line_min) {
      if (line_min < closest_line_after_block_offset) {
        closest_line_after = child.get();
        closest_line_after_block_offset = line_min;
      }
    }

    if (block_point >= line_max) {
      if (line_max > closest_line_before_block_offset) {
        closest_line_before = child.get();
        closest_line_before_block_offset = line_max;
      }
    }
  }

  if (closest_line_after) {
    if (auto child_position =
            PositionForPointInChild(*closest_line_after, point))
      return child_position.value();
  }

  if (closest_line_before) {
    if (auto child_position =
            PositionForPointInChild(*closest_line_before, point))
      return child_position.value();
  }

  // TODO(xiaochengh): Looking at only the closest lines may not be enough,
  // when we have multiple lines full of pseudo elements. Fix it.

  // TODO(xiaochengh): Consider floats.

  return PositionWithAffinity();
}

PositionWithAffinity NGPaintFragment::PositionForPoint(
    const NGPhysicalOffset& point) const {
  if (PhysicalFragment().IsText())
    return PositionForPointInText(point);

  if (PhysicalFragment().IsBlockFlow()) {
    // We current fall back to legacy for block formatting contexts, so we
    // should reach here only for inline formatting contexts.
    // TODO(xiaochengh): Do not fall back.
    DCHECK(ToNGPhysicalBoxFragment(PhysicalFragment()).ChildrenInline());
    return PositionForPointInInlineFormattingContext(point);
  }

  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
  return PositionForPointInInlineLevelBox(point);
}

Node* NGPaintFragment::NodeForHitTest() const {
  if (GetNode())
    return GetNode();

  if (PhysicalFragment().IsLineBox())
    return Parent()->NodeForHitTest();

  // When the fragment is inside a ::first-letter, ::before or ::after pseudo
  // node, return the pseudo node.
  for (const NGPaintFragment* runner = Parent(); runner;
       runner = runner->Parent()) {
    if (Node* node = runner->GetNode()) {
      if (CanBeHitTestTargetPseudoNode(*node))
        return node;
      return nullptr;
    }
  }

  return nullptr;
}

// ----

NGPaintFragment& NGPaintFragment::FragmentRange::front() const {
  DCHECK(first_);
  return *first_;
}

NGPaintFragment& NGPaintFragment::FragmentRange::back() const {
  DCHECK(first_);
  NGPaintFragment* last = first_;
  for (NGPaintFragment* fragment : *this)
    last = fragment;
  return *last;
}

}  // namespace blink
