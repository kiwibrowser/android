// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/caret_color.h"

#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* CaretColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueAuto)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
}

const blink::Color CaretColor::ColorIncludingFallback(
    bool visited_link,
    const ComputedStyle& style) const {
  StyleAutoColor auto_color =
      visited_link ? style.VisitedLinkCaretColor() : style.CaretColor();
  // TODO(rego): We may want to adjust the caret color if it's the same as
  // the background to ensure good visibility and contrast.
  StyleColor result = auto_color.IsAutoColor() ? StyleColor::CurrentColor()
                                               : auto_color.ToStyleColor();
  if (!result.IsCurrentColor())
    return result.GetColor();
  return visited_link ? style.VisitedLinkColor() : style.GetColor();
}

const CSSValue* CaretColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  blink::Color color;
  if (allow_visited_style)
    color = style.VisitedDependentColor(*this);
  else if (style.CaretColor().IsAutoColor())
    color = StyleColor::CurrentColor().Resolve(style.GetColor());
  else
    color = style.CaretColor().ToStyleColor().Resolve(style.GetColor());
  return cssvalue::CSSColorValue::Create(color.Rgb());
}

void CaretColor::ApplyInitial(StyleResolverState& state) const {
  StyleAutoColor color = StyleAutoColor::AutoColor();
  if (state.ApplyPropertyToRegularStyle())
    state.Style()->SetCaretColor(color);
  if (state.ApplyPropertyToVisitedLinkStyle())
    state.Style()->SetVisitedLinkCaretColor(color);
}

void CaretColor::ApplyInherit(StyleResolverState& state) const {
  StyleAutoColor color = state.ParentStyle()->CaretColor();
  if (state.ApplyPropertyToRegularStyle())
    state.Style()->SetCaretColor(color);
  if (state.ApplyPropertyToVisitedLinkStyle())
    state.Style()->SetVisitedLinkCaretColor(color);
}

void CaretColor::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  if (state.ApplyPropertyToRegularStyle()) {
    state.Style()->SetCaretColor(
        StyleBuilderConverter::ConvertStyleAutoColor(state, value));
  }
  if (state.ApplyPropertyToVisitedLinkStyle()) {
    state.Style()->SetVisitedLinkCaretColor(
        StyleBuilderConverter::ConvertStyleAutoColor(state, value, true));
  }
}

}  // namespace CSSLonghand
}  // namespace blink
