// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_child.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/layout/custom/css_layout_definition.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment_request.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

CustomLayoutChild::CustomLayoutChild(const CSSLayoutDefinition& definition,
                                     LayoutBox* box)
    : box_(box),
      style_map_(new PrepopulatedComputedStylePropertyMap(
          box->GetDocument(),
          box->StyleRef(),
          box->GetNode(),
          definition.ChildNativeInvalidationProperties(),
          definition.ChildCustomInvalidationProperties())) {}

CustomLayoutFragmentRequest* CustomLayoutChild::layoutNextFragment(
    ScriptState* script_state,
    const CustomLayoutConstraintsOptions& options,
    ExceptionState& exception_state) {
  // Serialize the provided data if needed.
  scoped_refptr<SerializedScriptValue> constraint_data;
  if (options.hasData()) {
    // We serialize "kForStorage" so that SharedArrayBuffers can't be shared
    // between LayoutWorkletGlobalScopes.
    constraint_data = SerializedScriptValue::Serialize(
        script_state->GetIsolate(), options.data().V8Value(),
        SerializedScriptValue::SerializeOptions(
            SerializedScriptValue::kForStorage),
        exception_state);

    if (exception_state.HadException())
      return nullptr;
  }

  return new CustomLayoutFragmentRequest(this, options,
                                         std::move(constraint_data));
}

void CustomLayoutChild::Trace(blink::Visitor* visitor) {
  visitor->Trace(style_map_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
