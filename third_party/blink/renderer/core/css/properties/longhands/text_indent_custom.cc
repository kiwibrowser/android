// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/text_indent.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* TextIndent::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // [ <length> | <percentage> ] && hanging? && each-line?
  // Keywords only allowed when css3Text is enabled.
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  bool has_length_or_percentage = false;
  bool has_each_line = false;
  bool has_hanging = false;

  do {
    if (!has_length_or_percentage) {
      if (CSSValue* text_indent =
              CSSPropertyParserHelpers::ConsumeLengthOrPercent(
                  range, context.Mode(), kValueRangeAll,
                  CSSPropertyParserHelpers::UnitlessQuirk::kAllow)) {
        list->Append(*text_indent);
        has_length_or_percentage = true;
        continue;
      }
    }

    if (RuntimeEnabledFeatures::CSS3TextEnabled()) {
      CSSValueID id = range.Peek().Id();
      if (!has_each_line && id == CSSValueEachLine) {
        list->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
        has_each_line = true;
        continue;
      }
      if (!has_hanging && id == CSSValueHanging) {
        list->Append(*CSSPropertyParserHelpers::ConsumeIdent(range));
        has_hanging = true;
        continue;
      }
    }
    return nullptr;
  } while (!range.AtEnd());

  if (!has_length_or_percentage)
    return nullptr;

  return list;
}

const CSSValue* TextIndent::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
      style.TextIndent(), style));
  if (RuntimeEnabledFeatures::CSS3TextEnabled() &&
      (style.GetTextIndentLine() == TextIndentLine::kEachLine ||
       style.GetTextIndentType() == TextIndentType::kHanging)) {
    if (style.GetTextIndentLine() == TextIndentLine::kEachLine)
      list->Append(*CSSIdentifierValue::Create(CSSValueEachLine));
    if (style.GetTextIndentType() == TextIndentType::kHanging)
      list->Append(*CSSIdentifierValue::Create(CSSValueHanging));
  }
  return list;
}

void TextIndent::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetTextIndent(ComputedStyleInitialValues::InitialTextIndent());
  state.Style()->SetTextIndentLine(
      ComputedStyleInitialValues::InitialTextIndentLine());
  state.Style()->SetTextIndentType(
      ComputedStyleInitialValues::InitialTextIndentType());
}

void TextIndent::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetTextIndent(state.ParentStyle()->TextIndent());
  state.Style()->SetTextIndentLine(state.ParentStyle()->GetTextIndentLine());
  state.Style()->SetTextIndentType(state.ParentStyle()->GetTextIndentType());
}

void TextIndent::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  Length length_or_percentage_value;
  TextIndentLine text_indent_line_value =
      ComputedStyleInitialValues::InitialTextIndentLine();
  TextIndentType text_indent_type_value =
      ComputedStyleInitialValues::InitialTextIndentType();

  for (auto& list_value : ToCSSValueList(value)) {
    if (list_value->IsPrimitiveValue()) {
      length_or_percentage_value =
          ToCSSPrimitiveValue(*list_value)
              .ConvertToLength(state.CssToLengthConversionData());
    } else if (ToCSSIdentifierValue(*list_value).GetValueID() ==
               CSSValueEachLine) {
      text_indent_line_value = TextIndentLine::kEachLine;
    } else if (ToCSSIdentifierValue(*list_value).GetValueID() ==
               CSSValueHanging) {
      text_indent_type_value = TextIndentType::kHanging;
    } else {
      NOTREACHED();
    }
  }

  state.Style()->SetTextIndent(length_or_percentage_value);
  state.Style()->SetTextIndentLine(text_indent_line_value);
  state.Style()->SetTextIndentType(text_indent_type_value);
}

}  // namespace CSSLonghand
}  // namespace blink
