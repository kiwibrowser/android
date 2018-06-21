// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

class NGPhysicalBoxFragmentTest : public NGLayoutTest {
 public:
  NGPhysicalBoxFragmentTest() : NGLayoutTest() {}

  const NGPhysicalBoxFragment& GetBodyFragment() const {
    return *ToLayoutBlockFlow(GetDocument().body()->GetLayoutObject())
                ->CurrentFragment();
  }
};

// TODO(layout-dev): Design more straightforward way to ensure old layout
// instead of using |contenteditable|.

// Tests that a normal old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_NormalOldLayoutRoot) {
  SetBodyInnerHTML("<div contenteditable>X</div>");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kNormalBox, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockLayoutRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that a float old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_FloatOldLayoutRoot) {
  SetBodyInnerHTML("<span contenteditable style='float:left'>X</span>foo");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kFloating, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockLayoutRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that an inline block old layout root box fragment has correct box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_InlineBlockOldLayoutRoot) {
  SetBodyInnerHTML(
      "<span contenteditable style='display:inline-block'>X</span>foo");
  const NGPhysicalContainerFragment* line_box =
      ToNGPhysicalContainerFragment(GetBodyFragment().Children().front().get());
  const NGPhysicalFragment* fragment = line_box->Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kAtomicInline, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockLayoutRoot());
}

// TODO(editing-dev): Once LayoutNG supports editing, we should change this
// test to use LayoutNG tree.
// Tests that an out-of-flow positioned old layout root box fragment has correct
// box type.
TEST_F(NGPhysicalBoxFragmentTest, DISABLED_OutOfFlowPositionedOldLayoutRoot) {
  SetBodyInnerHTML(
      "<style>body {position: absolute}</style>"
      "<div contenteditable style='position: absolute'>X</div>");
  const NGPhysicalFragment* fragment =
      GetBodyFragment().Children().front().get();
  ASSERT_TRUE(fragment);
  EXPECT_TRUE(fragment->IsBox());
  EXPECT_EQ(NGPhysicalFragment::kOutOfFlowPositioned, fragment->BoxType());
  EXPECT_TRUE(fragment->IsOldLayoutRoot());
  EXPECT_TRUE(fragment->IsBlockLayoutRoot());
}

}  // namespace blink
