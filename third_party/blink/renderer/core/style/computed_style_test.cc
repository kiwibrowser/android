// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/computed_style.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/style/clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"
#include "third_party/blink/renderer/core/style/style_difference.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"

namespace blink {

TEST(ComputedStyleTest, ShapeOutsideBoxEqual) {
  ShapeValue* shape1 = ShapeValue::CreateBoxShapeValue(CSSBoxType::kContent);
  ShapeValue* shape2 = ShapeValue::CreateBoxShapeValue(CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ShapeOutsideCircleEqual) {
  scoped_refptr<BasicShapeCircle> circle1 = BasicShapeCircle::Create();
  scoped_refptr<BasicShapeCircle> circle2 = BasicShapeCircle::Create();
  ShapeValue* shape1 =
      ShapeValue::CreateShapeValue(circle1, CSSBoxType::kContent);
  ShapeValue* shape2 =
      ShapeValue::CreateShapeValue(circle2, CSSBoxType::kContent);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetShapeOutside(shape1);
  style2->SetShapeOutside(shape2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, ClipPathEqual) {
  scoped_refptr<BasicShapeCircle> shape = BasicShapeCircle::Create();
  scoped_refptr<ShapeClipPathOperation> path1 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ShapeClipPathOperation> path2 =
      ShapeClipPathOperation::Create(shape);
  scoped_refptr<ComputedStyle> style1 = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> style2 = ComputedStyle::Create();
  style1->SetClipPath(path1);
  style2->SetClipPath(path2);
  EXPECT_EQ(*style1, *style2);
}

TEST(ComputedStyleTest, FocusRingWidth) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetEffectiveZoom(3.5);
#if defined(OS_MACOSX)
  style->SetOutlineStyle(EBorderStyle::kSolid);
  EXPECT_EQ(3, style->GetOutlineStrokeWidthForFocusRing());
#else
  EXPECT_EQ(3.5, style->GetOutlineStrokeWidthForFocusRing());
  style->SetEffectiveZoom(0.5);
  EXPECT_EQ(1, style->GetOutlineStrokeWidthForFocusRing());
#endif
}

TEST(ComputedStyleTest, FocusRingOutset) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetOutlineStyle(EBorderStyle::kSolid);
  style->SetOutlineStyleIsAuto(static_cast<bool>(OutlineIsAuto::kOn));
  style->SetEffectiveZoom(4.75);
#if defined(OS_MACOSX)
  EXPECT_EQ(4, style->OutlineOutsetExtent());
#else
  EXPECT_EQ(3, style->OutlineOutsetExtent());
#endif
}

TEST(ComputedStyleTest, SVGStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->UpdateIsStackingContext(false, false, true);
  EXPECT_TRUE(style->IsStackingContext());
}

TEST(ComputedStyleTest, Preserve3dForceStackingContext) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  style->SetOverflowX(EOverflow::kHidden);
  style->SetOverflowY(EOverflow::kHidden);
  style->UpdateIsStackingContext(false, false, false);
  EXPECT_EQ(ETransformStyle3D::kFlat, style->UsedTransformStyle3D());
  EXPECT_TRUE(style->IsStackingContext());
}

TEST(ComputedStyleTest, FirstPublicPseudoStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetHasPseudoStyle(kPseudoIdFirstLine);
  EXPECT_TRUE(style->HasPseudoStyle(kPseudoIdFirstLine));
  EXPECT_TRUE(style->HasAnyPublicPseudoStyles());
}

TEST(ComputedStyleTest, LastPublicPseudoStyle) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetHasPseudoStyle(kPseudoIdScrollbar);
  EXPECT_TRUE(style->HasPseudoStyle(kPseudoIdScrollbar));
  EXPECT_TRUE(style->HasAnyPublicPseudoStyles());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesRespectsTransformAnimation) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);
  other->SetHasCurrentTransformAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.TransformChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsTransforom) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  TransformOperations operations(true);
  style->SetTransform(operations);
  other->SetTransform(operations);

  other->SetHasCurrentTransformAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_FALSE(diff.TransformChanged());
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsOpacity) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentOpacityAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsFilter) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentFilterAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsBackdropFilter) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasCurrentBackdropFilterAnimation(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsInlineTransform) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetHasInlineTransform(true);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsBackfaceVisibility) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsWillChange) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetBackfaceVisibility(EBackfaceVisibility::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsUsedStylePreserve3D) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetTransformStyle3D(ETransformStyle3D::kPreserve3d);
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  // This induces a flat used transform style.
  other->SetOpacity(0.5);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsOverflow) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  other->SetOverflowX(EOverflow::kHidden);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest,
     UpdatePropertySpecificDifferencesCompositingReasonsContainsPaint) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Clone(*style);

  // This induces a flat used transform style.
  other->SetContain(kContainsPaint);
  StyleDifference diff;
  style->UpdatePropertySpecificDifferences(*other, diff);
  EXPECT_TRUE(diff.CompositingReasonsChanged());
}

TEST(ComputedStyleTest, HasOutlineWithCurrentColor) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->HasOutline());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineWidth(5);
  EXPECT_FALSE(style->HasOutlineWithCurrentColor());
  style->SetOutlineStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasOutlineWithCurrentColor());
}

TEST(ComputedStyleTest, HasBorderColorReferencingCurrentColor) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomColor(StyleColor::CurrentColor());
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomWidth(5);
  EXPECT_FALSE(style->HasBorderColorReferencingCurrentColor());
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_TRUE(style->HasBorderColorReferencingCurrentColor());
}

TEST(ComputedStyleTest, BorderWidth) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  style->SetBorderBottomWidth(5);
  EXPECT_EQ(style->BorderBottomWidth(), 0);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
  style->SetBorderBottomStyle(EBorderStyle::kSolid);
  EXPECT_EQ(style->BorderBottomWidth(), 5);
  EXPECT_EQ(style->BorderBottom().Width(), 5);
}

TEST(ComputedStyleTest, CursorList) {
  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();
  scoped_refptr<ComputedStyle> other = ComputedStyle::Create();

  cssvalue::CSSGradientValue* gradient =
      cssvalue::CSSLinearGradientValue::Create(
          nullptr, nullptr, nullptr, nullptr, nullptr, cssvalue::kRepeating);

  StyleImage* image_value = StyleGeneratedImage::Create(*gradient);
  StyleImage* other_image_value = StyleGeneratedImage::Create(*gradient);

  EXPECT_TRUE(DataEquivalent(image_value, other_image_value));

  style->AddCursor(image_value, false);
  other->AddCursor(other_image_value, false);
  EXPECT_EQ(*style, *other);
}

}  // namespace blink
