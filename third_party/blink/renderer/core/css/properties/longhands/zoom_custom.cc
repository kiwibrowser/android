// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/zoom.h"

#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Zoom::ParseSingleValue(CSSParserTokenRange& range,
                                       const CSSParserContext& context,
                                       const CSSParserLocalContext&) const {
  const CSSParserToken& token = range.Peek();
  CSSValue* zoom = nullptr;
  if (token.GetType() == kIdentToken) {
    zoom = CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal>(range);
  } else {
    zoom =
        CSSPropertyParserHelpers::ConsumePercent(range, kValueRangeNonNegative);
    if (!zoom) {
      zoom = CSSPropertyParserHelpers::ConsumeNumber(range,
                                                     kValueRangeNonNegative);
    }
  }
  if (zoom) {
    if (!(token.Id() == CSSValueNormal ||
          (token.GetType() == kNumberToken &&
           ToCSSPrimitiveValue(zoom)->GetDoubleValue() == 1) ||
          (token.GetType() == kPercentageToken &&
           ToCSSPrimitiveValue(zoom)->GetDoubleValue() == 100)))
      context.Count(WebFeature::kCSSZoomNotEqualToOne);
  }
  return zoom;
}

const CSSValue* Zoom::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSPrimitiveValue::Create(style.Zoom(),
                                   CSSPrimitiveValue::UnitType::kNumber);
}

void Zoom::ApplyInitial(StyleResolverState& state) const {
  state.SetZoom(ComputedStyleInitialValues::InitialZoom());
}

void Zoom::ApplyInherit(StyleResolverState& state) const {
  state.SetZoom(state.ParentStyle()->Zoom());
}

void Zoom::ApplyValue(StyleResolverState& state, const CSSValue& value) const {
  SECURITY_DCHECK(value.IsPrimitiveValue() || value.IsIdentifierValue());

  if (value.IsIdentifierValue()) {
    const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);
    if (identifier_value.GetValueID() == CSSValueNormal) {
      state.SetZoom(ComputedStyleInitialValues::InitialZoom());
    }
  } else if (value.IsPrimitiveValue()) {
    const CSSPrimitiveValue& primitive_value = ToCSSPrimitiveValue(value);
    if (primitive_value.IsPercentage()) {
      if (float percent = primitive_value.GetFloatValue())
        state.SetZoom(percent / 100.0f);
      else
        state.SetZoom(1.0f);
    } else if (primitive_value.IsNumber()) {
      if (float number = primitive_value.GetFloatValue())
        state.SetZoom(number);
      else
        state.SetZoom(1.0f);
    }
  }
}

}  // namespace CSSLonghand
}  // namespace blink
