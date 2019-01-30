// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/size.h"

#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"

namespace blink {
namespace CSSLonghand {

static CSSValue* ConsumePageSize(CSSParserTokenRange& range) {
  return CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueA3, CSSValueA4, CSSValueA5, CSSValueB4, CSSValueB5,
      CSSValueLedger, CSSValueLegal, CSSValueLetter>(range);
}

static float MmToPx(float mm) {
  return mm * kCssPixelsPerMillimeter;
}
static float InchToPx(float inch) {
  return inch * kCssPixelsPerInch;
}
static FloatSize GetPageSizeFromName(const CSSIdentifierValue& page_size_name) {
  switch (page_size_name.GetValueID()) {
    case CSSValueA5:
      return FloatSize(MmToPx(148), MmToPx(210));
    case CSSValueA4:
      return FloatSize(MmToPx(210), MmToPx(297));
    case CSSValueA3:
      return FloatSize(MmToPx(297), MmToPx(420));
    case CSSValueB5:
      return FloatSize(MmToPx(176), MmToPx(250));
    case CSSValueB4:
      return FloatSize(MmToPx(250), MmToPx(353));
    case CSSValueLetter:
      return FloatSize(InchToPx(8.5), InchToPx(11));
    case CSSValueLegal:
      return FloatSize(InchToPx(8.5), InchToPx(14));
    case CSSValueLedger:
      return FloatSize(InchToPx(11), InchToPx(17));
    default:
      NOTREACHED();
      return FloatSize(0, 0);
  }
}

const CSSValue* Size::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();

  if (range.Peek().Id() == CSSValueAuto) {
    result->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
    return result;
  }

  if (CSSValue* width = CSSPropertyParserHelpers::ConsumeLength(
          range, context.Mode(), kValueRangeNonNegative)) {
    CSSValue* height = CSSPropertyParserHelpers::ConsumeLength(
        range, context.Mode(), kValueRangeNonNegative);
    result->Append(*width);
    if (height)
      result->Append(*height);
    return result;
  }

  CSSValue* page_size = ConsumePageSize(range);
  CSSValue* orientation =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValuePortrait,
                                             CSSValueLandscape>(range);
  if (!page_size)
    page_size = ConsumePageSize(range);

  if (!orientation && !page_size)
    return nullptr;
  if (page_size)
    result->Append(*page_size);
  if (orientation)
    result->Append(*orientation);
  return result;
}

void Size::ApplyInitial(StyleResolverState& state) const {}

void Size::ApplyInherit(StyleResolverState& state) const {}

void Size::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  state.Style()->ResetPageSizeType();
  FloatSize size;
  EPageSizeType page_size_type = EPageSizeType::kAuto;
  const CSSValueList& list = ToCSSValueList(value);
  if (list.length() == 2) {
    // <length>{2} | <page-size> <orientation>
    const CSSValue& first = list.Item(0);
    const CSSValue& second = list.Item(1);
    if (first.IsPrimitiveValue() && ToCSSPrimitiveValue(first).IsLength()) {
      // <length>{2}
      size = FloatSize(
          ToCSSPrimitiveValue(first).ComputeLength<float>(
              state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0)),
          ToCSSPrimitiveValue(second).ComputeLength<float>(
              state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0)));
    } else {
      // <page-size> <orientation>
      size = GetPageSizeFromName(ToCSSIdentifierValue(first));

      DCHECK(ToCSSIdentifierValue(second).GetValueID() == CSSValueLandscape ||
             ToCSSIdentifierValue(second).GetValueID() == CSSValuePortrait);
      if (ToCSSIdentifierValue(second).GetValueID() == CSSValueLandscape)
        size = size.TransposedSize();
    }
    page_size_type = EPageSizeType::kResolved;
  } else {
    DCHECK_EQ(list.length(), 1U);
    // <length> | auto | <page-size> | [ portrait | landscape]
    const CSSValue& first = list.Item(0);
    if (first.IsPrimitiveValue() && ToCSSPrimitiveValue(first).IsLength()) {
      // <length>
      page_size_type = EPageSizeType::kResolved;
      float width = ToCSSPrimitiveValue(first).ComputeLength<float>(
          state.CssToLengthConversionData().CopyWithAdjustedZoom(1.0));
      size = FloatSize(width, width);
    } else {
      const CSSIdentifierValue& ident = ToCSSIdentifierValue(first);
      switch (ident.GetValueID()) {
        case CSSValueAuto:
          page_size_type = EPageSizeType::kAuto;
          break;
        case CSSValuePortrait:
          page_size_type = EPageSizeType::kPortrait;
          break;
        case CSSValueLandscape:
          page_size_type = EPageSizeType::kLandscape;
          break;
        default:
          // <page-size>
          page_size_type = EPageSizeType::kResolved;
          size = GetPageSizeFromName(ident);
      }
    }
  }
  state.Style()->SetPageSizeType(page_size_type);
  state.Style()->SetPageSize(size);
}

}  // namespace CSSLonghand
}  // namespace blink
