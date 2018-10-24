// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_position.h"

#include "third_party/blink/renderer/core/dom/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/modules/accessibility/ax_layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

// static
const AXPosition AXPosition::CreatePositionBeforeObject(const AXObject& child) {
  // If |child| is a text object, make behavior the same as
  // |CreateFirstPositionInObject| so that equality would hold.
  if (child.GetNode() && child.GetNode()->IsTextNode())
    return CreateFirstPositionInObject(child);

  const AXObject* parent = child.ParentObjectUnignored();
  DCHECK(parent);
  AXPosition position(*parent);
  position.text_offset_or_child_index_ = child.IndexInParent();
  DCHECK(position.IsValid());
  return position.AsUnignoredPosition();
}

// static
const AXPosition AXPosition::CreatePositionAfterObject(const AXObject& child) {
  // If |child| is a text object, make behavior the same as
  // |CreateLastPositionInObject| so that equality would hold.
  if (child.GetNode() && child.GetNode()->IsTextNode())
    return CreateLastPositionInObject(child);

  const AXObject* parent = child.ParentObjectUnignored();
  DCHECK(parent);
  AXPosition position(*parent);
  position.text_offset_or_child_index_ = child.IndexInParent() + 1;
  DCHECK(position.IsValid());
  return position.AsUnignoredPosition();
}

// static
const AXPosition AXPosition::CreateFirstPositionInObject(
    const AXObject& container) {
  if (container.GetNode() && container.GetNode()->IsTextNode()) {
    AXPosition position(container);
    position.text_offset_or_child_index_ = 0;
    DCHECK(position.IsValid());
    return position.AsUnignoredPosition();
  }

  const AXObject* unignored_container = container.AccessibilityIsIgnored()
                                            ? container.ParentObjectUnignored()
                                            : &container;
  DCHECK(unignored_container);
  AXPosition position(*unignored_container);
  position.text_offset_or_child_index_ = 0;
  DCHECK(position.IsValid());
  return position.AsUnignoredPosition();
}

// static
const AXPosition AXPosition::CreateLastPositionInObject(
    const AXObject& container) {
  if (container.GetNode() && container.GetNode()->IsTextNode()) {
    AXPosition position(container);
    position.text_offset_or_child_index_ = position.MaxTextOffset();
    DCHECK(position.IsValid());
    return position.AsUnignoredPosition();
  }

  const AXObject* unignored_container = container.AccessibilityIsIgnored()
                                            ? container.ParentObjectUnignored()
                                            : &container;
  DCHECK(unignored_container);
  AXPosition position(*unignored_container);
  position.text_offset_or_child_index_ = unignored_container->ChildCount();
  DCHECK(position.IsValid());
  return position.AsUnignoredPosition();
}

// static
const AXPosition AXPosition::CreatePositionInTextObject(
    const AXObject& container,
    const int offset,
    const TextAffinity affinity) {
  DCHECK(container.GetNode() && container.GetNode()->IsTextNode())
      << "Text positions should be anchored to a text node.";
  AXPosition position(container);
  position.text_offset_or_child_index_ = offset;
  position.affinity_ = affinity;
  DCHECK(position.IsValid());
  return position.AsUnignoredPosition();
}

// static
const AXPosition AXPosition::FromPosition(const Position& position,
                                          const TextAffinity affinity) {
  if (position.IsNull() || position.IsOrphan())
    return {};

  const Document* document = position.GetDocument();
  // Non orphan positions always have a document.
  DCHECK(document);

  AXObjectCache* ax_object_cache = document->ExistingAXObjectCache();
  if (!ax_object_cache)
    return {};

  auto* ax_object_cache_impl = static_cast<AXObjectCacheImpl*>(ax_object_cache);
  const Position& parent_anchored_position = position.ToOffsetInAnchor();
  const Node* anchor_node = parent_anchored_position.AnchorNode();
  DCHECK(anchor_node);
  const AXObject* container = ax_object_cache_impl->GetOrCreate(anchor_node);
  DCHECK(container);

  AXPosition ax_position(*container);
  if (anchor_node->IsTextNode()) {
    // Convert from a DOM offset that may have uncompressed white space to a
    // character offset.
    // TODO(nektar): Use LayoutNG offset mapping instead of
    // |TextIterator|.
    const auto first_position = Position::FirstPositionInNode(*anchor_node);
    int offset =
        TextIterator::RangeLength(first_position, parent_anchored_position);
    ax_position.text_offset_or_child_index_ = offset;
  } else {
    // |ComputeNodeAfterPosition| returns nullptr for "after children"
    // positions.
    const Node* node_after_position = position.ComputeNodeAfterPosition();
    if (!node_after_position) {
      ax_position.text_offset_or_child_index_ = container->ChildCount();
    } else {
      const AXObject* ax_child =
          ax_object_cache_impl->GetOrCreate(node_after_position);
      DCHECK(ax_child);
      if (ax_child->IsDescendantOf(*container)) {
        ax_position.text_offset_or_child_index_ = ax_child->IndexInParent();
      } else {
        return CreatePositionBeforeObject(*ax_child);
      }
    }
  }

  ax_position.affinity_ = affinity;
  DCHECK(ax_position.IsValid());
  return ax_position.AsUnignoredPosition();
}

// static
const AXPosition AXPosition::FromPosition(
    const PositionWithAffinity& position_with_affinity) {
  return FromPosition(position_with_affinity.GetPosition(),
                      position_with_affinity.Affinity());
}

// Only for use by |AXSelection| to represent empty selection ranges.
AXPosition::AXPosition()
    : container_object_(nullptr),
      text_offset_or_child_index_(0),
      affinity_(TextAffinity::kDownstream) {
#if DCHECK_IS_ON()
  dom_tree_version_ = 0;
  style_version_ = 0;
#endif
}

AXPosition::AXPosition(const AXObject& container)
    : container_object_(&container),
      text_offset_or_child_index_(0),
      affinity_(TextAffinity::kDownstream) {
  const Document* document = container_object_->GetDocument();
  DCHECK(document);
#if DCHECK_IS_ON()
  dom_tree_version_ = document->DomTreeVersion();
  style_version_ = document->StyleVersion();
#endif
}

const AXObject* AXPosition::ChildAfterTreePosition() const {
  if (!IsValid() || IsTextPosition())
    return nullptr;
  if (container_object_->ChildCount() <= ChildIndex())
    return nullptr;
  return *(container_object_->Children().begin() + ChildIndex());
}

int AXPosition::ChildIndex() const {
  if (!IsTextPosition())
    return text_offset_or_child_index_;
  NOTREACHED() << *this << " should be a tree position.";
  return 0;
}

int AXPosition::TextOffset() const {
  if (IsTextPosition())
    return text_offset_or_child_index_;
  NOTREACHED() << *this << " should be a text position.";
  return 0;
}

int AXPosition::MaxTextOffset() const {
  if (!IsTextPosition()) {
    NOTREACHED() << *this << " should be a text position.";
    return 0;
  }

  // TODO(nektar): Use LayoutNG offset mapping instead of |TextIterator|.
  const auto first_position =
      Position::FirstPositionInNode(*container_object_->GetNode());
  const auto last_position =
      Position::LastPositionInNode(*container_object_->GetNode());
  return TextIterator::RangeLength(first_position, last_position);
}

bool AXPosition::IsValid() const {
  if (!container_object_ || container_object_->IsDetached())
    return false;
  if (!container_object_->GetNode() ||
      !container_object_->GetNode()->isConnected()) {
    return false;
  }

  if (!container_object_->GetNode()->IsTextNode()) {
    if (text_offset_or_child_index_ > container_object_->ChildCount())
      return false;
  }

  DCHECK(container_object_->GetNode()->GetDocument().IsActive());
  DCHECK(!container_object_->GetNode()->GetDocument().NeedsLayoutTreeUpdate());
#if DCHECK_IS_ON()
  DCHECK_EQ(container_object_->GetNode()->GetDocument().DomTreeVersion(),
            dom_tree_version_);
  DCHECK_EQ(container_object_->GetNode()->GetDocument().StyleVersion(),
            style_version_);
#endif  // DCHECK_IS_ON()
  return true;
}

bool AXPosition::IsTextPosition() const {
  return container_object_ && container_object_->GetNode()->IsTextNode();
}

const AXPosition AXPosition::CreateNextPosition() const {
  if (!IsValid())
    return {};

  if (IsTextPosition() && TextOffset() < MaxTextOffset())
    return CreatePositionInTextObject(*container_object_, TextOffset() + 1);

  const AXObject* child = ChildAfterTreePosition();
  // Handles both an "after children" position, or a text position that is after
  // the last character.
  if (!child) {
    const AXObject* next_in_order = container_object_->NextInTreeObject();
    if (!next_in_order || !next_in_order->ParentObjectUnignored())
      return {};
    return CreatePositionBeforeObject(*next_in_order);
  }

  if (!child->ParentObjectUnignored())
    return {};
  return CreatePositionAfterObject(*child);
}

const AXPosition AXPosition::CreatePreviousPosition() const {
  if (!IsValid())
    return {};

  if (IsTextPosition() && TextOffset() > 0)
    return CreatePositionInTextObject(*container_object_, TextOffset() - 1);

  const AXObject* child = ChildAfterTreePosition();
  // Handles both an "after children" position, or a text position that is
  // before the first character.
  if (!child) {
    if (container_object_->ChildCount()) {
      const AXObject* last_child = container_object_->LastChild();
      // Dont skip over any intervening text.
      if (last_child->GetNode() && last_child->GetNode()->IsTextNode())
        return CreatePositionAfterObject(*last_child);

      return CreatePositionBeforeObject(*last_child);
    }

    const AXObject* previous_in_order =
        container_object_->PreviousInTreeObject();
    if (!previous_in_order || !previous_in_order->ParentObjectUnignored())
      return {};
    return CreatePositionAfterObject(*previous_in_order);
  }

  const AXObject* object_before_position = child->PreviousInTreeObject();
  if (!object_before_position ||
      !object_before_position->ParentObjectUnignored()) {
    return {};
  }

  // Dont skip over any intervening text.
  if (object_before_position->GetNode() &&
      object_before_position->GetNode()->IsTextNode()) {
    return CreatePositionAfterObject(*object_before_position);
  }

  return CreatePositionBeforeObject(*object_before_position);
}

const AXPosition AXPosition::AsUnignoredPosition(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  if (!IsValid())
    return {};

  // There are four possibilities:
  // 1. The container object is ignored and this is not a text position or an
  // "after children" position. Try to find the equivalent position in the
  // unignored parent.
  // 2. The container object is ignored and this is a text
  // position. Adjust to the position immediately to the left or to the right,
  // based on the adjustment behavior, possibly changing to a non-text position
  // and recurse. 3. The position is an "after children" position, but the last
  // child is ignored. Do the same as 2. 4. The object after the position is
  // ignored, but the container object is not. Do the same as 2.

  const AXObject* container = container_object_;
  const AXObject* child = ChildAfterTreePosition();
  const AXObject* last_child = container->LastChild();

  // Case 1.
  if (container->AccessibilityIsIgnored() && child) {
    return CreatePositionBeforeObject(*child).AsUnignoredPosition(
        adjustment_behavior);
  }

  // Cases 2, 3 and 4.
  if (container->AccessibilityIsIgnored() ||
      (!child && last_child && last_child->AccessibilityIsIgnored()) ||
      (child && child->AccessibilityIsIgnored())) {
    switch (adjustment_behavior) {
      case AXPositionAdjustmentBehavior::kMoveRight:
        return CreateNextPosition().AsUnignoredPosition(adjustment_behavior);
      case AXPositionAdjustmentBehavior::kMoveLeft:
        return CreatePreviousPosition().AsUnignoredPosition(
            adjustment_behavior);
    }
  }

  return *this;
}

const AXPosition AXPosition::AsValidDOMPosition(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  if (!IsValid())
    return {};

  // We adjust to the next or previous position if the container or the child
  // object after a tree position are mock or virtual objects, since mock or
  // virtual objects will not be present in the DOM tree. Alternatively, in the
  // case of an "after children" position, we need to check if the last child of
  // the container object is mock or virtual and adjust accordingly.

  // More Explaination:
  // If the child after a tree position doesn't have an associated node in the
  // DOM tree, we adjust to the next or previous position because a
  // corresponding child node will not be found in the DOM tree. We need a
  // corresponding child node in the DOM tree so that we can anchor the DOM
  // position before it. We can't ask the layout tree for the child's container
  // block node, because this might change the placement of the AX position
  // drastically. However, if the container doesn't have a corresponding DOM
  // node, we need to use the layout tree to find its corresponding container
  // block node, because no AX positions inside an anonymous layout block could
  // be represented in the DOM tree anyway.

  const AXObject* container = container_object_;
  DCHECK(container);
  const AXObject* child = ChildAfterTreePosition();
  const AXObject* last_child = container->LastChild();
  if (container->IsMockObject() || container->IsVirtualObject() ||
      (!child && last_child &&
       (!last_child->GetNode() || last_child->IsMockObject() ||
        last_child->IsVirtualObject())) ||
      (child && (!child->GetNode() || child->IsMockObject() ||
                 child->IsVirtualObject()))) {
    switch (adjustment_behavior) {
      case AXPositionAdjustmentBehavior::kMoveRight:
        return CreateNextPosition().AsValidDOMPosition(adjustment_behavior);
      case AXPositionAdjustmentBehavior::kMoveLeft:
        return CreatePreviousPosition().AsValidDOMPosition(adjustment_behavior);
    }
  }

  if (container->GetNode())
    return this->AsUnignoredPosition(adjustment_behavior);

  DCHECK(container->IsAXLayoutObject())
      << "Non virtual and non mock AX objects that are not associated to a DOM "
         "node should have an associated layout object.";
  const Node* anchor_node =
      ToAXLayoutObject(container)->GetNodeOrContainingBlockNode();
  DCHECK(anchor_node)
      << "All anonymous layout objects should have a containing block element.";
  DCHECK(!container->IsDetached());
  auto& ax_object_cache_impl = container->AXObjectCache();
  const AXObject* new_container = ax_object_cache_impl.GetOrCreate(anchor_node);
  DCHECK(new_container);
  AXPosition position(*new_container);
  if (new_container == container->ParentObjectUnignored()) {
    position.text_offset_or_child_index_ = container->IndexInParent();
  } else {
    position.text_offset_or_child_index_ = 0;
  }
  DCHECK(position.IsValid());
  return position.AsValidDOMPosition(adjustment_behavior);
}

const PositionWithAffinity AXPosition::ToPositionWithAffinity(
    const AXPositionAdjustmentBehavior adjustment_behavior) const {
  const AXPosition adjusted_position = AsValidDOMPosition();
  if (!adjusted_position.IsValid())
    return {};

  const Node* container_node = adjusted_position.container_object_->GetNode();
  DCHECK(container_node);
  if (!adjusted_position.IsTextPosition()) {
    // AX positions that are unumbiguously at the start or end of a container,
    // should convert to the corresponding DOM positions at the start or end of
    // the same container. Other child positions in the accessibility tree
    // should recompute their parent in the DOM tree, because they might be ARIA
    // owned by a different object in the accessibility tree than in the DOM
    // tree, or their parent in the accessibility tree might be ignored.

    if (adjusted_position.ChildIndex() == 0) {
      // Creates a |PositionAnchorType::kBeforeChildren| position.
      return PositionWithAffinity(
          Position::FirstPositionInNode(*container_node), affinity_);
    }

    if (adjusted_position.ChildIndex() == container_object_->ChildCount()) {
      // Creates a |PositionAnchorType::kAfterChildren| position.
      return PositionWithAffinity(Position::LastPositionInNode(*container_node),
                                  affinity_);
    }

    // Creates a |PositionAnchorType::kOffsetInAnchor| position.
    const AXObject* ax_child =
        *(adjusted_position.container_object_->Children().begin() +
          adjusted_position.ChildIndex());
    DCHECK(ax_child);
    return PositionWithAffinity(
        Position::InParentBeforeNode(*(ax_child->GetNode())), affinity_);
  }

  // TODO(nektar): Use LayoutNG offset mapping instead of |TextIterator|.
  const auto first_position = Position::FirstPositionInNode(*container_node);
  const auto last_position = Position::LastPositionInNode(*container_node);
  CharacterIterator character_iterator(first_position, last_position);
  const EphemeralRange range = character_iterator.CalculateCharacterSubrange(
      0, adjusted_position.text_offset_or_child_index_);
  return PositionWithAffinity(range.EndPosition(), affinity_);
}

bool operator==(const AXPosition& a, const AXPosition& b) {
  DCHECK(a.IsValid() && b.IsValid());
  if (*a.ContainerObject() != *b.ContainerObject())
    return false;
  if (a.IsTextPosition() && b.IsTextPosition())
    return a.TextOffset() == b.TextOffset() && a.Affinity() == b.Affinity();
  if (!a.IsTextPosition() && !b.IsTextPosition())
    return a.ChildIndex() == b.ChildIndex();
  NOTREACHED() << "AXPosition objects having the same container object should "
                  "have the same type.";
  return false;
}

bool operator!=(const AXPosition& a, const AXPosition& b) {
  return !(a == b);
}

bool operator<(const AXPosition& a, const AXPosition& b) {
  DCHECK(a.IsValid() && b.IsValid());

  if (a.ContainerObject() == b.ContainerObject()) {
    if (a.IsTextPosition() && b.IsTextPosition())
      return a.TextOffset() < b.TextOffset();
    if (!a.IsTextPosition() && !b.IsTextPosition())
      return a.ChildIndex() < b.ChildIndex();
    NOTREACHED()
        << "AXPosition objects having the same container object should "
           "have the same type.";
    return false;
  }

  int index_in_ancestor1, index_in_ancestor2;
  const AXObject* ancestor =
      AXObject::LowestCommonAncestor(*a.ContainerObject(), *b.ContainerObject(),
                                     &index_in_ancestor1, &index_in_ancestor2);
  DCHECK_GE(index_in_ancestor1, -1);
  DCHECK_GE(index_in_ancestor2, -1);
  if (!ancestor)
    return false;
  if (ancestor == a.ContainerObject()) {
    DCHECK(!a.IsTextPosition());
    index_in_ancestor1 = a.ChildIndex();
  }
  if (ancestor == b.ContainerObject()) {
    DCHECK(!b.IsTextPosition());
    index_in_ancestor2 = b.ChildIndex();
  }
  return index_in_ancestor1 < index_in_ancestor2;
}

bool operator<=(const AXPosition& a, const AXPosition& b) {
  return a < b || a == b;
}

bool operator>(const AXPosition& a, const AXPosition& b) {
  DCHECK(a.IsValid() && b.IsValid());

  if (a.ContainerObject() == b.ContainerObject()) {
    if (a.IsTextPosition() && b.IsTextPosition())
      return a.TextOffset() > b.TextOffset();
    if (!a.IsTextPosition() && !b.IsTextPosition())
      return a.ChildIndex() > b.ChildIndex();
    NOTREACHED()
        << "AXPosition objects having the same container object should "
           "have the same type.";
    return false;
  }

  int index_in_ancestor1, index_in_ancestor2;
  const AXObject* ancestor =
      AXObject::LowestCommonAncestor(*a.ContainerObject(), *b.ContainerObject(),
                                     &index_in_ancestor1, &index_in_ancestor2);
  DCHECK_GE(index_in_ancestor1, -1);
  DCHECK_GE(index_in_ancestor2, -1);
  if (!ancestor)
    return false;
  if (ancestor == a.ContainerObject()) {
    DCHECK(!a.IsTextPosition());
    index_in_ancestor1 = a.ChildIndex();
  }
  if (ancestor == b.ContainerObject()) {
    DCHECK(!b.IsTextPosition());
    index_in_ancestor2 = b.ChildIndex();
  }
  return index_in_ancestor1 > index_in_ancestor2;
}

bool operator>=(const AXPosition& a, const AXPosition& b) {
  return a > b || a == b;
}

std::ostream& operator<<(std::ostream& ostream, const AXPosition& position) {
  if (!position.IsValid())
    return ostream << "Invalid AXPosition";
  if (position.IsTextPosition()) {
    return ostream << "AX text position in " << *position.ContainerObject()
                   << ", " << position.TextOffset();
  }
  return ostream << "AX object anchored position in "
                 << *position.ContainerObject() << ", "
                 << position.ChildIndex();
}

}  // namespace blink
