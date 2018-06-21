// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_text_emphasis_style.h"

#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* WebkitTextEmphasisStyle::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);

  if (CSSValue* text_emphasis_style =
          CSSPropertyParserHelpers::ConsumeString(range))
    return text_emphasis_style;

  CSSIdentifierValue* fill =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueFilled, CSSValueOpen>(
          range);
  CSSIdentifierValue* shape =
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueDot, CSSValueCircle,
                                             CSSValueDoubleCircle,
                                             CSSValueTriangle, CSSValueSesame>(
          range);
  if (!fill) {
    fill = CSSPropertyParserHelpers::ConsumeIdent<CSSValueFilled, CSSValueOpen>(
        range);
  }
  if (fill && shape) {
    CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
    parsed_values->Append(*fill);
    parsed_values->Append(*shape);
    return parsed_values;
  }
  if (fill)
    return fill;
  if (shape)
    return shape;
  return nullptr;
}

const CSSValue* WebkitTextEmphasisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  switch (style.GetTextEmphasisMark()) {
    case TextEmphasisMark::kNone:
      return CSSIdentifierValue::Create(CSSValueNone);
    case TextEmphasisMark::kCustom:
      return CSSStringValue::Create(style.TextEmphasisCustomMark());
    case TextEmphasisMark::kAuto:
      NOTREACHED();
      FALLTHROUGH;
    case TextEmphasisMark::kDot:
    case TextEmphasisMark::kCircle:
    case TextEmphasisMark::kDoubleCircle:
    case TextEmphasisMark::kTriangle:
    case TextEmphasisMark::kSesame: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
      return list;
    }
  }
  NOTREACHED();
  return nullptr;
}

void WebkitTextEmphasisStyle::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetTextEmphasisFill(
      ComputedStyleInitialValues::InitialTextEmphasisFill());
  state.Style()->SetTextEmphasisMark(
      ComputedStyleInitialValues::InitialTextEmphasisMark());
  state.Style()->SetTextEmphasisCustomMark(
      ComputedStyleInitialValues::InitialTextEmphasisCustomMark());
}

void WebkitTextEmphasisStyle::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetTextEmphasisFill(
      state.ParentStyle()->GetTextEmphasisFill());
  state.Style()->SetTextEmphasisMark(
      state.ParentStyle()->GetTextEmphasisMark());
  state.Style()->SetTextEmphasisCustomMark(
      state.ParentStyle()->TextEmphasisCustomMark());
}

void WebkitTextEmphasisStyle::ApplyValue(StyleResolverState& state,
                                         const CSSValue& value) const {
  if (value.IsValueList()) {
    const CSSValueList& list = ToCSSValueList(value);
    DCHECK_EQ(list.length(), 2U);
    for (unsigned i = 0; i < 2; ++i) {
      const CSSIdentifierValue& value = ToCSSIdentifierValue(list.Item(i));
      if (value.GetValueID() == CSSValueFilled ||
          value.GetValueID() == CSSValueOpen)
        state.Style()->SetTextEmphasisFill(value.ConvertTo<TextEmphasisFill>());
      else
        state.Style()->SetTextEmphasisMark(value.ConvertTo<TextEmphasisMark>());
    }
    state.Style()->SetTextEmphasisCustomMark(g_null_atom);
    return;
  }

  if (value.IsStringValue()) {
    state.Style()->SetTextEmphasisFill(TextEmphasisFill::kFilled);
    state.Style()->SetTextEmphasisMark(TextEmphasisMark::kCustom);
    state.Style()->SetTextEmphasisCustomMark(
        AtomicString(ToCSSStringValue(value).Value()));
    return;
  }

  const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);

  state.Style()->SetTextEmphasisCustomMark(g_null_atom);

  if (identifier_value.GetValueID() == CSSValueFilled ||
      identifier_value.GetValueID() == CSSValueOpen) {
    state.Style()->SetTextEmphasisFill(
        identifier_value.ConvertTo<TextEmphasisFill>());
    state.Style()->SetTextEmphasisMark(TextEmphasisMark::kAuto);
  } else {
    state.Style()->SetTextEmphasisFill(TextEmphasisFill::kFilled);
    state.Style()->SetTextEmphasisMark(
        identifier_value.ConvertTo<TextEmphasisMark>());
  }
}

}  // namespace CSSLonghand
}  // namespace blink
