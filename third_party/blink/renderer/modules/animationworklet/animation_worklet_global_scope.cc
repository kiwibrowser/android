// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/animation_worklet_global_scope.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/animation_worklet_proxy_client.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

void UpdateAnimation(Animator* animator,
                     ScriptState* script_state,
                     int id,
                     double current_time,
                     CompositorMutatorOutputState* result) {
  CompositorMutatorOutputState::AnimationState animation_output;
  if (animator->Animate(script_state, current_time, &animation_output)) {
    animation_output.animation_id = id;
    result->animations.push_back(std::move(animation_output));
  }
}

}  // namespace

AnimationWorkletGlobalScope* AnimationWorkletGlobalScope::Create(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread) {
  return new AnimationWorkletGlobalScope(std::move(creation_params), isolate,
                                         thread);
}

AnimationWorkletGlobalScope::AnimationWorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    v8::Isolate* isolate,
    WorkerThread* thread)
    : ThreadedWorkletGlobalScope(std::move(creation_params), isolate, thread) {
}

AnimationWorkletGlobalScope::~AnimationWorkletGlobalScope() = default;

void AnimationWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(animator_definitions_);
  visitor->Trace(animators_);
  ThreadedWorkletGlobalScope::Trace(visitor);
}

void AnimationWorkletGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients()))
    proxy_client->Dispose();
  ThreadedWorkletGlobalScope::Dispose();
}

Animator* AnimationWorkletGlobalScope::CreateAnimatorFor(
    int animation_id,
    const String& name,
    WorkletAnimationOptions* options) {
  DCHECK(!animators_.at(animation_id));
  Animator* animator = CreateInstance(name, options);
  if (!animator)
    return nullptr;
  animators_.Set(animation_id, animator);

  return animator;
}

std::unique_ptr<CompositorMutatorOutputState>
AnimationWorkletGlobalScope::Mutate(
    const CompositorMutatorInputState& mutator_input) {
  DCHECK(IsContextThread());

  ScriptState* script_state = ScriptController()->GetScriptState();
  ScriptState::Scope scope(script_state);

  std::unique_ptr<CompositorMutatorOutputState> result =
      std::make_unique<CompositorMutatorOutputState>();

  for (const auto& id : mutator_input.removed_animations)
    animators_.erase(id);

  for (const auto& animation : mutator_input.added_and_updated_animations) {
    int id = animation.animation_id;
    DCHECK(!animators_.at(id));
    const String name =
        String::FromUTF8(animation.name.data(), animation.name.size());

    // Down casting to blink type to access the serialized value.
    WorkletAnimationOptions* options =
        static_cast<WorkletAnimationOptions*>(animation.options.get());

    Animator* animator = CreateAnimatorFor(id, name, options);
    if (!animator)
      continue;

    UpdateAnimation(animator, script_state, id, animation.current_time,
                    result.get());
  }

  for (const auto& animation : mutator_input.updated_animations) {
    int id = animation.animation_id;
    Animator* animator = animators_.at(id);
    // We don't try to create an animator if there isn't any.
    if (!animator)
      continue;

    UpdateAnimation(animator, script_state, id, animation.current_time,
                    result.get());
  }

  return result;
}

void AnimationWorkletGlobalScope::RegisterWithProxyClientIfNeeded() {
  if (registered_)
    return;

  if (AnimationWorkletProxyClient* proxy_client =
          AnimationWorkletProxyClient::From(Clients())) {
    proxy_client->SetGlobalScope(this);
    registered_ = true;
  }
}

void AnimationWorkletGlobalScope::registerAnimator(
    const String& name,
    const ScriptValue& constructor_value,
    ExceptionState& exception_state) {
  RegisterWithProxyClientIfNeeded();

  DCHECK(IsContextThread());
  if (animator_definitions_.Contains(name)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "A class with name:'" + name + "' is already registered.");
    return;
  }

  if (name.IsEmpty()) {
    exception_state.ThrowTypeError("The empty string is not a valid name.");
    return;
  }

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Context> context = ScriptController()->GetContext();

  DCHECK(constructor_value.V8Value()->IsFunction());
  v8::Local<v8::Function> constructor =
      v8::Local<v8::Function>::Cast(constructor_value.V8Value());

  v8::Local<v8::Object> prototype;
  if (!V8ObjectParser::ParsePrototype(context, constructor, &prototype,
                                      &exception_state))
    return;

  v8::Local<v8::Function> animate;
  if (!V8ObjectParser::ParseFunction(context, prototype, "animate", &animate,
                                     &exception_state))
    return;

  AnimatorDefinition* definition =
      new AnimatorDefinition(isolate, constructor, animate);

  animator_definitions_.Set(name, definition);
}

Animator* AnimationWorkletGlobalScope::CreateInstance(
    const String& name,
    WorkletAnimationOptions* options) {
  DCHECK(IsContextThread());
  AnimatorDefinition* definition = animator_definitions_.at(name);
  if (!definition)
    return nullptr;

  v8::Isolate* isolate = ScriptController()->GetScriptState()->GetIsolate();
  v8::Local<v8::Function> constructor = definition->ConstructorLocal(isolate);
  DCHECK(!IsUndefinedOrNull(constructor));
  v8::Local<v8::Value> value;
  if (options && options->GetData())
    value = options->GetData()->Deserialize(isolate);

  v8::Local<v8::Object> instance;
  if (!V8ObjectConstructor::NewInstance(isolate, constructor,
                                        !value.IsEmpty() ? 1 : 0, &value)
           .ToLocal(&instance))
    return nullptr;

  return new Animator(isolate, definition, instance);
}

AnimatorDefinition* AnimationWorkletGlobalScope::FindDefinitionForTest(
    const String& name) {
  return animator_definitions_.at(name);
}

}  // namespace blink
