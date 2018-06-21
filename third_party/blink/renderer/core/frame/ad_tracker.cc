// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_tracker.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/CoreProbeSink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

AdTracker::AdTracker(LocalFrame* local_root) : local_root_(local_root) {
  local_root_->GetProbeSink()->addAdTracker(this);
}

AdTracker::~AdTracker() {
  DCHECK(!local_root_);
}

void AdTracker::Shutdown() {
  if (!local_root_)
    return;
  local_root_->GetProbeSink()->removeAdTracker(this);
  local_root_ = nullptr;
}

String AdTracker::ScriptAtTopOfStack(ExecutionContext* execution_context) {
  std::unique_ptr<blink::SourceLocation> current_stack_trace =
      SourceLocation::Capture(execution_context);
  // TODO(jkarlin): Url() sometimes returns String(), why?
  return current_stack_trace ? current_stack_trace->Url() : "";
}

ExecutionContext* AdTracker::GetCurrentExecutionContext() {
  // Determine the current ExecutionContext.
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  return context.IsEmpty() ? nullptr : ToExecutionContext(context);
}

void AdTracker::WillExecuteScript(ExecutionContext* execution_context,
                                  const String& script_url) {
  bool is_ad = script_url.IsEmpty()
                   ? false
                   : IsKnownAdScript(execution_context, script_url);
  ExecutingScript script(script_url, is_ad);
  executing_scripts_.push_back(script);
}

void AdTracker::DidExecuteScript() {
  executing_scripts_.pop_back();
}

void AdTracker::Will(const probe::ExecuteScript& probe) {
  WillExecuteScript(probe.context, probe.script_url);
}

void AdTracker::Did(const probe::ExecuteScript& probe) {
  DidExecuteScript();
}

void AdTracker::Will(const probe::CallFunction& probe) {
  // Do not process nested microtasks as that might potentially lead to a
  // slowdown of custom element callbacks.
  if (probe.depth)
    return;

  v8::Local<v8::Value> resource_name =
      probe.function->GetScriptOrigin().ResourceName();
  String script_url;
  if (!resource_name.IsEmpty())
    script_url = ToCoreString(resource_name->ToString());
  WillExecuteScript(probe.context, script_url);
}

void AdTracker::Did(const probe::CallFunction& probe) {
  if (probe.depth)
    return;

  DidExecuteScript();
}

void AdTracker::WillSendRequest(ExecutionContext* execution_context,
                                unsigned long identifier,
                                DocumentLoader* loader,
                                ResourceRequest& request,
                                const ResourceResponse& redirect_response,
                                const FetchInitiatorInfo& initiator_info,
                                Resource::Type resource_type) {
  // If the resource is not already marked as an ad, check if any executing
  // script is an ad. If yes, mark this as an ad.
  if (!request.IsAdResource() && IsAdScriptInStack())
    request.SetIsAdResource();

  // If it is a script marked as an ad, append it to the known ad scripts set.
  if (resource_type == Resource::kScript && request.IsAdResource()) {
    AppendToKnownAdScripts(*execution_context, request.Url().GetString());
  }
}

bool AdTracker::IsAdScriptInStack() {
  ExecutionContext* execution_context = GetCurrentExecutionContext();
  if (!execution_context)
    return false;

  // The pseudo-stack contains entry points into the stack (e.g., when v8 is
  // executed) but not the entire stack. It's cheap to retrieve the top of the
  // stack so scan that as well.
  String top_script = ScriptAtTopOfStack(execution_context);

  if (!top_script.IsEmpty() && IsKnownAdScript(execution_context, top_script))
    return true;

  // Scan the pseudo-stack for ad scripts.
  for (const auto& executing_script : executing_scripts_) {
    if (executing_script.is_ad)
      return true;
  }
  return false;
}

bool AdTracker::IsKnownAdScript(ExecutionContext* execution_context,
                                const String& url) {
  if (!execution_context)
    return false;

  auto it = known_ad_scripts_.find(execution_context);
  if (it == known_ad_scripts_.end())
    return false;
  return it->value.find(url) != it->value.end();
}

// This is a separate function for testing purposes.
void AdTracker::AppendToKnownAdScripts(ExecutionContext& execution_context,
                                       const String& url) {
  auto add_result =
      known_ad_scripts_.insert(&execution_context, HashSet<String>());
  add_result.stored_value->value.insert(url);
}

void AdTracker::Trace(blink::Visitor* visitor) {
  visitor->Trace(local_root_);
  visitor->Trace(known_ad_scripts_);
}

}  // namespace blink
