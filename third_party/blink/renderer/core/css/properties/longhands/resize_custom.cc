// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/resize.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Resize::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSIdentifierValue::Create(style.Resize());
}

void Resize::ApplyValue(StyleResolverState& state,
                        const CSSValue& value) const {
  const CSSIdentifierValue& identifier_value = ToCSSIdentifierValue(value);

  EResize r = EResize::kNone;
  if (identifier_value.GetValueID() == CSSValueAuto) {
    if (Settings* settings = state.GetDocument().GetSettings()) {
      r = settings->GetTextAreasAreResizable() ? EResize::kBoth
                                               : EResize::kNone;
    }
    UseCounter::Count(state.GetDocument(), WebFeature::kCSSResizeAuto);
  } else {
    r = identifier_value.ConvertTo<EResize>();
  }
  state.Style()->SetResize(r);
}

}  // namespace CSSLonghand
}  // namespace blink
