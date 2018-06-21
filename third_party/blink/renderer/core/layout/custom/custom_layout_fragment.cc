// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/layout/custom/custom_layout_fragment_request.h"
#include "third_party/blink/renderer/core/layout/custom/layout_custom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

CustomLayoutFragment::CustomLayoutFragment(
    CustomLayoutFragmentRequest* fragment_request,
    const LayoutUnit inline_size,
    const LayoutUnit block_size,
    v8::Isolate* isolate)
    : fragment_request_(fragment_request),
      inline_size_(inline_size.ToDouble()),
      block_size_(block_size.ToDouble()) {
  // Immediately store the result data, so that it remains immutable between
  // layout calls to the child.
  if (GetLayoutBox()->IsLayoutCustom()) {
    SerializedScriptValue* data =
        ToLayoutCustom(GetLayoutBox())->GetFragmentResultData();
    if (data)
      layout_worklet_world_v8_data_.Set(isolate, data->Deserialize(isolate));
  }
}

LayoutBox* CustomLayoutFragment::GetLayoutBox() const {
  return fragment_request_->GetLayoutBox();
}

bool CustomLayoutFragment::IsValid() const {
  return fragment_request_->IsValid();
}

ScriptValue CustomLayoutFragment::data(ScriptState* script_state) const {
  // "data" is *only* exposed to the LayoutWorkletGlobalScope, and we are able
  // to return the same deserialized object. We don't need to check which world
  // it is being accessed from.
  DCHECK(ExecutionContext::From(script_state)->IsLayoutWorkletGlobalScope());
  DCHECK(script_state->World().IsWorkerWorld());

  if (layout_worklet_world_v8_data_.IsEmpty())
    return ScriptValue::CreateNull(script_state);

  return ScriptValue(script_state, layout_worklet_world_v8_data_.NewLocal(
                                       script_state->GetIsolate()));
}

void CustomLayoutFragment::Trace(blink::Visitor* visitor) {
  visitor->Trace(fragment_request_);
  visitor->Trace(layout_worklet_world_v8_data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
