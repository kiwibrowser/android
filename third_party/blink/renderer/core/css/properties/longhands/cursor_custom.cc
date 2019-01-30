// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/cursor.h"

#include "third_party/blink/renderer/core/css/css_cursor_image_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_mode.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Cursor::ParseSingleValue(CSSParserTokenRange& range,
                                         const CSSParserContext& context,
                                         const CSSParserLocalContext&) const {
  bool in_quirks_mode = IsQuirksModeBehavior(context.Mode());
  CSSValueList* list = nullptr;
  while (CSSValue* image = CSSPropertyParserHelpers::ConsumeImage(
             range, &context,
             CSSPropertyParserHelpers::ConsumeGeneratedImagePolicy::kForbid)) {
    double num;
    IntPoint hot_spot(-1, -1);
    bool hot_spot_specified = false;
    if (CSSPropertyParserHelpers::ConsumeNumberRaw(range, num)) {
      hot_spot.SetX(clampTo<int>(num));
      if (!CSSPropertyParserHelpers::ConsumeNumberRaw(range, num))
        return nullptr;
      hot_spot.SetY(clampTo<int>(num));
      hot_spot_specified = true;
    }

    if (!list)
      list = CSSValueList::CreateCommaSeparated();

    list->Append(*cssvalue::CSSCursorImageValue::Create(
        *image, hot_spot_specified, hot_spot));
    if (!CSSPropertyParserHelpers::ConsumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  CSSValueID id = range.Peek().Id();
  if (!range.AtEnd()) {
    if (id == CSSValueWebkitZoomIn)
      context.Count(WebFeature::kPrefixedCursorZoomIn);
    else if (id == CSSValueWebkitZoomOut)
      context.Count(WebFeature::kPrefixedCursorZoomOut);
    else if (id == CSSValueWebkitGrab)
      context.Count(WebFeature::kPrefixedCursorGrab);
    else if (id == CSSValueWebkitGrabbing)
      context.Count(WebFeature::kPrefixedCursorGrabbing);
  }
  CSSValue* cursor_type = nullptr;
  if (id == CSSValueHand) {
    if (!in_quirks_mode)  // Non-standard behavior
      return nullptr;
    cursor_type = CSSIdentifierValue::Create(CSSValuePointer);
    range.ConsumeIncludingWhitespace();
  } else if ((id >= CSSValueAuto && id <= CSSValueWebkitZoomOut) ||
             id == CSSValueCopy || id == CSSValueNone) {
    cursor_type = CSSPropertyParserHelpers::ConsumeIdent(range);
  } else {
    return nullptr;
  }

  if (!list)
    return cursor_type;
  list->Append(*cursor_type);
  return list;
}

const CSSValue* Cursor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = nullptr;
  CursorList* cursors = style.Cursors();
  if (cursors && cursors->size() > 0) {
    list = CSSValueList::CreateCommaSeparated();
    for (const CursorData& cursor : *cursors) {
      if (StyleImage* image = cursor.GetImage()) {
        list->Append(*cssvalue::CSSCursorImageValue::Create(
            *image->ComputedCSSValue(), cursor.HotSpotSpecified(),
            cursor.HotSpot()));
      }
    }
  }
  CSSValue* value = CSSIdentifierValue::Create(style.Cursor());
  if (list) {
    list->Append(*value);
    return list;
  }
  return value;
}

void Cursor::ApplyInitial(StyleResolverState& state) const {
  state.Style()->ClearCursorList();
  state.Style()->SetCursor(ComputedStyleInitialValues::InitialCursor());
}

void Cursor::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetCursor(state.ParentStyle()->Cursor());
  state.Style()->SetCursorList(state.ParentStyle()->Cursors());
}

void Cursor::ApplyValue(StyleResolverState& state,
                        const CSSValue& value) const {
  state.Style()->ClearCursorList();
  if (value.IsValueList()) {
    state.Style()->SetCursor(ECursor::kAuto);
    for (const auto& item : ToCSSValueList(value)) {
      if (item->IsCursorImageValue()) {
        const cssvalue::CSSCursorImageValue& cursor =
            cssvalue::ToCSSCursorImageValue(*item);
        const CSSValue& image = cursor.ImageValue();
        state.Style()->AddCursor(state.GetStyleImage(CSSPropertyCursor, image),
                                 cursor.HotSpotSpecified(), cursor.HotSpot());
      } else {
        state.Style()->SetCursor(
            ToCSSIdentifierValue(*item).ConvertTo<ECursor>());
      }
    }
  } else {
    state.Style()->SetCursor(ToCSSIdentifierValue(value).ConvertTo<ECursor>());
  }
}

}  // namespace CSSLonghand
}  // namespace blink
