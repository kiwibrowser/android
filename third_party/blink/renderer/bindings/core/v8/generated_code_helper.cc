// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

void V8ConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    const WrapperTypeInfo* wrapper_type_info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(
      info.GetIsolate(), "Blink_V8ConstructorAttributeGetter");
  V8PerContextData* per_context_data =
      V8PerContextData::From(info.Holder()->CreationContext());
  if (!per_context_data)
    return;
  V8SetReturnValue(info,
                   per_context_data->ConstructorForType(wrapper_type_info));
}

v8::Local<v8::Value> V8Deserialize(v8::Isolate* isolate,
                                   SerializedScriptValue* value) {
  if (value)
    return value->Deserialize(isolate);
  return v8::Null(isolate);
}

bool IsCallbackFunctionRunnable(
    const ScriptState* callback_relevant_script_state,
    ScriptState* incumbent_script_state) {
  if (!callback_relevant_script_state->ContextIsValid())
    return false;
  const ExecutionContext* relevant_execution_context =
      ExecutionContext::From(callback_relevant_script_state);
  if (!relevant_execution_context ||
      relevant_execution_context->IsContextPaused() ||
      relevant_execution_context->IsContextDestroyed()) {
    return false;
  }

  // TODO(yukishiino): Callback function type value must make the incumbent
  // environment alive, i.e. the reference to v8::Context must be strong.
  v8::HandleScope handle_scope(incumbent_script_state->GetIsolate());
  v8::Local<v8::Context> incumbent_context =
      incumbent_script_state->GetContext();
  ExecutionContext* incumbent_execution_context =
      incumbent_context.IsEmpty() ? nullptr
                                  : ToExecutionContext(incumbent_context);
  // The incumbent realm schedules the currently-running callback although it
  // may not correspond to the currently-running function object. So we check
  // the incumbent context which originally schedules the currently-running
  // callback to see whether the script setting is disabled before invoking
  // the callback.
  return incumbent_execution_context &&
         !incumbent_execution_context->IsContextPaused() &&
         !incumbent_execution_context->IsContextDestroyed() &&
         incumbent_execution_context->CanExecuteScripts(kAboutToExecuteScript);
}

}  // namespace blink
