// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/worker_module_script_fetcher.h"

#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/referrer_policy.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

WorkerModuleScriptFetcher::WorkerModuleScriptFetcher(
    WorkerGlobalScope* global_scope)
    : global_scope_(global_scope) {}

// https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
void WorkerModuleScriptFetcher::Fetch(FetchParameters& fetch_params,
                                      ModuleGraphLevel level,
                                      ModuleScriptFetcher::Client* client) {
  DCHECK(global_scope_->IsContextThread());
  client_ = client;
  level_ = level;

  // Step 13. "In both cases, to perform the fetch given request, perform the
  // following steps if the is top-level flag is set:" [spec text]
  // Step 13.1. "Set request's reserved client to inside settings." [spec text]
  // This is implemented in the browser process.

  // Step 13.2. "Fetch request, and asynchronously wait to run the remaining
  // steps as part of fetch's process response for the response response." [spec
  // text]
  ScriptResource::Fetch(fetch_params, global_scope_->EnsureFetcher(), this);
}

void WorkerModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  ModuleScriptFetcher::Trace(visitor);
  visitor->Trace(client_);
  visitor->Trace(global_scope_);
}

// https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
void WorkerModuleScriptFetcher::NotifyFinished(Resource* resource) {
  DCHECK(global_scope_->IsContextThread());
  ClearResource();

  ScriptResource* script_resource = ToScriptResource(resource);
  HeapVector<Member<ConsoleMessage>> error_messages;
  if (!WasModuleLoadSuccessful(script_resource, &error_messages)) {
    client_->NotifyFetchFinished(base::nullopt, error_messages);
    return;
  }

  // TODO(nhiroki): This branch condition must be satisfied only for worker's
  // top-level module script fetch, but actually this is also satisfied for
  // dynamic import because it's also defined as top-level module script fetch
  // in the HTML spec. This results in overriding the referrer policy of the
  // global scope. We should fix this before enabling module workers by default.
  // (https://crbug.com/842553)
  if (level_ == ModuleGraphLevel::kTopLevelModuleFetch) {
    // TODO(nhiroki, hiroshige): Access to WorkerGlobalScope in module loaders
    // is a layering violation. Also, updating WorkerGlobalScope ('module map
    // settigns object') in flight can be dangerous because module loaders may
    // refers to it. We should move these steps out of core/loader/modulescript/
    // and run them after module loading. This may require the spec change.
    // (https://crbug.com/845285)

    // Step 13.3. "Set worker global scope's url to response's url." [spec text]
    // Step 13.4. "Set worker global scope's HTTPS state to response's HTTPS
    // state." [spec text]

    // Step 13.5. "Set worker global scope's referrer policy to the result of
    // parsing the `Referrer-Policy` header of response." [spec text]
    const String referrer_policy_header =
        resource->GetResponse().HttpHeaderField(HTTPNames::Referrer_Policy);
    if (!referrer_policy_header.IsNull()) {
      ReferrerPolicy referrer_policy = kReferrerPolicyDefault;
      SecurityPolicy::ReferrerPolicyFromHeaderValue(
          referrer_policy_header, kDoNotSupportReferrerPolicyLegacyKeywords,
          &referrer_policy);
      global_scope_->SetReferrerPolicy(referrer_policy);
    }

    // Step 13.6. "Execute the Initialize a global object's CSP list algorithm
    // on worker global scope and response. [CSP]" [spec text]
    // This is done in the constructor of WorkerGlobalScope.
  }

  ModuleScriptCreationParams params(
      script_resource->GetResponse().Url(), script_resource->SourceText(),
      script_resource->GetResourceRequest().GetFetchCredentialsMode(),
      script_resource->CalculateAccessControlStatus(
          global_scope_->EnsureFetcher()->Context().GetSecurityOrigin()));

  // Step 13.7. "Asynchronously complete the perform the fetch steps with
  // response." [spec text]
  client_->NotifyFetchFinished(params, error_messages);
}

}  // namespace blink
