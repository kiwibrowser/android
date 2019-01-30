// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

namespace blink {

namespace {

// Preorder traverse |container|, and collect the fragments satisfying
// |filter| into |results|.
// |filter|.IsTraverse(NGPaintFragment) returns true to traverse children.
// |filter|.IsCollectible(NGPaintFragment) returns true to collect fragment.

template <typename Filter>
void CollectPaintFragments(const NGPaintFragment& container,
                           NGPhysicalOffset offset_to_container_box,
                           Filter& filter,
                           Vector<NGPaintFragmentWithContainerOffset>* result) {
  for (const auto& child : container.Children()) {
    NGPaintFragmentWithContainerOffset fragment_with_offset{
        child.get(), child->Offset() + offset_to_container_box};
    if (filter.IsCollectible(child.get())) {
      result->push_back(fragment_with_offset);
    }
    if (filter.IsTraverse(child.get())) {
      CollectPaintFragments(*child.get(), fragment_with_offset.container_offset,
                            filter, result);
    }
  }
}

// Does not collect fragments with SelfPaintingLayer or their descendants.
class NotSelfPaintingFilter {
 public:
  bool IsCollectible(const NGPaintFragment* fragment) const {
    return !fragment->HasSelfPaintingLayer();
  }
  bool IsTraverse(const NGPaintFragment* fragment) const {
    return !fragment->HasSelfPaintingLayer();
  }
};

// Collects line box and inline fragments.
class InlineFilter {
 public:
  bool IsCollectible(const NGPaintFragment* fragment) const {
    return fragment->PhysicalFragment().IsInline() ||
           fragment->PhysicalFragment().IsLineBox();
  }
  bool IsTraverse(const NGPaintFragment* fragment) {
    return fragment->PhysicalFragment().IsContainer() &&
           !fragment->PhysicalFragment().IsBlockLayoutRoot();
  }
};

// Collect only fragments that belong to this LayoutObject.
class LayoutObjectFilter {
 public:
  explicit LayoutObjectFilter(const LayoutObject* layout_object)
      : layout_object_(layout_object) {
    DCHECK(layout_object);
  };
  bool IsCollectible(const NGPaintFragment* fragment) const {
    return fragment->GetLayoutObject() == layout_object_;
  }
  bool IsTraverse(const NGPaintFragment*) const { return true; }

 private:
  const LayoutObject* layout_object_;
};

// ------ Helpers for traversing inline fragments ------

bool IsLineBreak(const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  const NGPhysicalFragment& physical_fragment =
      fragment.GetFragment()->PhysicalFragment();
  DCHECK(physical_fragment.IsInline());
  if (!physical_fragment.IsText())
    return false;
  return ToNGPhysicalTextFragment(physical_fragment).IsLineBreak();
}

bool IsInlineLeaf(const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  const NGPhysicalFragment& physical_fragment =
      fragment.GetFragment()->PhysicalFragment();
  if (!physical_fragment.IsInline())
    return false;
  return physical_fragment.IsText() || physical_fragment.IsAtomicInline();
}

NGPaintFragmentTraversalContext FirstInclusiveLeafDescendantOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  if (IsInlineLeaf(fragment))
    return fragment;
  const auto& children = fragment.GetFragment()->Children();
  for (unsigned i = 0; i < children.size(); ++i) {
    NGPaintFragmentTraversalContext maybe_leaf =
        FirstInclusiveLeafDescendantOf({fragment.GetFragment(), i});
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  return NGPaintFragmentTraversalContext();
}

NGPaintFragmentTraversalContext LastInclusiveLeafDescendantOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  if (IsInlineLeaf(fragment))
    return fragment;
  const auto& children = fragment.GetFragment()->Children();
  for (unsigned i = children.size(); i != 0u; --i) {
    NGPaintFragmentTraversalContext maybe_leaf =
        LastInclusiveLeafDescendantOf({fragment.GetFragment(), i - 1});
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  return NGPaintFragmentTraversalContext();
}

NGPaintFragmentTraversalContext PreviousSiblingOf(
    const NGPaintFragmentTraversalContext& fragment) {
  if (!fragment.parent || fragment.index == 0u)
    return NGPaintFragmentTraversalContext();
  return {fragment.parent, fragment.index - 1};
}

NGPaintFragmentTraversalContext NextSiblingOf(
    const NGPaintFragmentTraversalContext& fragment) {
  if (!fragment.parent)
    return NGPaintFragmentTraversalContext();
  if (fragment.index + 1 == fragment.parent->Children().size())
    return NGPaintFragmentTraversalContext();
  return {fragment.parent, fragment.index + 1};
}

unsigned IndexOfChild(const NGPaintFragment& parent,
                      const NGPaintFragment& fragment) {
  const auto& children = parent.Children();
  const auto* it = std::find_if(
      children.begin(), children.end(),
      [&fragment](const auto& child) { return &fragment == child.get(); });
  DCHECK(it != children.end());
  return std::distance(children.begin(), it);
}

}  // namespace

NGPaintFragmentTraversal::NGPaintFragmentTraversal(const NGPaintFragment& root)
    : root_(root) {
  Push(root, 0);
}

NGPaintFragmentTraversal::NGPaintFragmentTraversal(const NGPaintFragment& root,
                                                   const NGPaintFragment& start)
    : root_(root) {
  MoveTo(start);
}

void NGPaintFragmentTraversal::Push(const NGPaintFragment& parent,
                                    unsigned index) {
  stack_.push_back(ParentAndIndex{&parent, index});
  current_ = parent.Children()[index].get();
}

void NGPaintFragmentTraversal::Push(const NGPaintFragment& fragment) {
  const NGPaintFragment* parent = fragment.Parent();
  DCHECK(parent);
  Push(*parent, IndexOfChild(*parent, fragment));
}

void NGPaintFragmentTraversal::MoveTo(const NGPaintFragment& fragment) {
  DCHECK(fragment.IsDescendantOfNotSelf(root_));

  // Because we may not traverse all descendants of |root_|, just push the
  // specified fragment. Computing its ancestors up to |root_| is deferred to
  // |MoveToNextSiblingOrAncestor()|.
  stack_.resize(0);
  Push(fragment);
}

void NGPaintFragmentTraversal::MoveToNext() {
  if (IsAtEnd())
    return;

  if (!current_->Children().IsEmpty()) {
    Push(*current_, 0);
    return;
  }

  MoveToNextSiblingOrAncestor();
}

void NGPaintFragmentTraversal::MoveToNextSiblingOrAncestor() {
  while (!IsAtEnd()) {
    // Check if we have a next sibling.
    auto& stack_top = stack_.back();
    if (++stack_top.index < stack_top.parent->Children().size()) {
      current_ = stack_top.parent->Children()[stack_top.index].get();
      return;
    }
    MoveToParent();
  }
}

void NGPaintFragmentTraversal::MoveToParent() {
  if (IsAtEnd())
    return;
  DCHECK(!stack_.IsEmpty());
  const NGPaintFragment& parent = *stack_.back().parent;
  stack_.pop_back();
  if (&parent == &root_) {
    DCHECK(stack_.IsEmpty());
    current_ = nullptr;
    return;
  }
  if (stack_.IsEmpty()) {
    // We might have started with |MoveTo()|, and thus computing parent stack
    // was deferred.
    Push(parent);
    return;
  }
  DCHECK_EQ(&parent,
            stack_.back().parent->Children()[stack_.back().index].get());
  current_ = &parent;
}

void NGPaintFragmentTraversal::MoveToPrevious() {
  if (IsAtEnd())
    return;
  DCHECK(!stack_.IsEmpty());
  auto& stack_top = stack_.back();
  if (stack_top.index == 0) {
    // There is no previous sibling of |current_|. We move to parent.
    MoveToParent();
    return;
  }
  --stack_top.index;
  current_ = stack_top.parent->Children()[stack_top.index].get();
  while (!current_->Children().IsEmpty())
    Push(*current_, current_->Children().size() - 1);
}

Vector<NGPaintFragmentWithContainerOffset>
NGPaintFragmentTraversal::DescendantsOf(const NGPaintFragment& container) {
  Vector<NGPaintFragmentWithContainerOffset> result;
  NotSelfPaintingFilter filter;
  CollectPaintFragments(container, NGPhysicalOffset(), filter, &result);
  return result;
}

Vector<NGPaintFragmentWithContainerOffset>
NGPaintFragmentTraversal::InlineDescendantsOf(
    const NGPaintFragment& container) {
  Vector<NGPaintFragmentWithContainerOffset> result;
  InlineFilter filter;
  CollectPaintFragments(container, NGPhysicalOffset(), filter, &result);
  return result;
}

Vector<NGPaintFragmentWithContainerOffset>
NGPaintFragmentTraversal::SelfFragmentsOf(const NGPaintFragment& container,
                                          const LayoutObject* target) {
  Vector<NGPaintFragmentWithContainerOffset> result;
  LayoutObjectFilter filter(target);
  CollectPaintFragments(container, NGPhysicalOffset(), filter, &result);
  return result;
}

NGPaintFragment* NGPaintFragmentTraversal::PreviousLineOf(
    const NGPaintFragment& line) {
  DCHECK(line.PhysicalFragment().IsLineBox());
  NGPaintFragment* parent = line.Parent();
  DCHECK(parent);
  NGPaintFragment* previous_line = nullptr;
  for (const auto& sibling : parent->Children()) {
    if (sibling.get() == &line)
      return previous_line;
    if (sibling->PhysicalFragment().IsLineBox())
      previous_line = sibling.get();
  }
  NOTREACHED();
  return nullptr;
}

const NGPaintFragment* NGPaintFragmentTraversalContext::GetFragment() const {
  if (!parent)
    return nullptr;
  return parent->Children()[index].get();
}

// static
NGPaintFragmentTraversalContext NGPaintFragmentTraversalContext::Create(
    const NGPaintFragment* fragment) {
  if (!fragment)
    return NGPaintFragmentTraversalContext();
  return {fragment->Parent(), IndexOfChild(*fragment->Parent(), *fragment)};
}

NGPaintFragmentTraversalContext NGPaintFragmentTraversal::PreviousInlineLeafOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  DCHECK(fragment.GetFragment()->PhysicalFragment().IsInline());
  for (auto sibling = PreviousSiblingOf(fragment); !sibling.IsNull();
       sibling = PreviousSiblingOf(sibling)) {
    NGPaintFragmentTraversalContext maybe_leaf =
        LastInclusiveLeafDescendantOf(sibling);
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  DCHECK(fragment.parent);
  if (fragment.parent->PhysicalFragment().IsLineBox())
    return NGPaintFragmentTraversalContext();
  return PreviousInlineLeafOf(
      NGPaintFragmentTraversalContext::Create(fragment.parent));
}

NGPaintFragmentTraversalContext NGPaintFragmentTraversal::NextInlineLeafOf(
    const NGPaintFragmentTraversalContext& fragment) {
  DCHECK(!fragment.IsNull());
  DCHECK(fragment.GetFragment()->PhysicalFragment().IsInline());
  for (auto sibling = NextSiblingOf(fragment); !sibling.IsNull();
       sibling = NextSiblingOf(sibling)) {
    NGPaintFragmentTraversalContext maybe_leaf =
        FirstInclusiveLeafDescendantOf(sibling);
    if (!maybe_leaf.IsNull())
      return maybe_leaf;
  }
  DCHECK(fragment.parent);
  if (fragment.parent->PhysicalFragment().IsLineBox())
    return NGPaintFragmentTraversalContext();
  return NextInlineLeafOf(
      NGPaintFragmentTraversalContext::Create(fragment.parent));
}

NGPaintFragmentTraversalContext
NGPaintFragmentTraversal::PreviousInlineLeafOfIgnoringLineBreak(
    const NGPaintFragmentTraversalContext& fragment) {
  NGPaintFragmentTraversalContext runner = PreviousInlineLeafOf(fragment);
  while (!runner.IsNull() && IsLineBreak(runner))
    runner = PreviousInlineLeafOf(runner);
  return runner;
}

NGPaintFragmentTraversalContext
NGPaintFragmentTraversal::NextInlineLeafOfIgnoringLineBreak(
    const NGPaintFragmentTraversalContext& fragment) {
  NGPaintFragmentTraversalContext runner = NextInlineLeafOf(fragment);
  while (!runner.IsNull() && IsLineBreak(runner))
    runner = NextInlineLeafOf(runner);
  return runner;
}

}  // namespace blink
