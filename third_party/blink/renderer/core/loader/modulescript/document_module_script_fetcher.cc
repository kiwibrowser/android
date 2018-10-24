// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/document_module_script_fetcher.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/script/layered_api.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

DocumentModuleScriptFetcher::DocumentModuleScriptFetcher(
    ResourceFetcher* fetcher)
    : fetcher_(fetcher) {
  DCHECK(fetcher_);
}

void DocumentModuleScriptFetcher::Fetch(FetchParameters& fetch_params,
                                        ModuleGraphLevel level,
                                        ModuleScriptFetcher::Client* client) {
  DCHECK(!client_);
  client_ = client;

  if (FetchIfLayeredAPI(fetch_params))
    return;

  ScriptResource::Fetch(fetch_params, fetcher_, this);
}

void DocumentModuleScriptFetcher::NotifyFinished(Resource* resource) {
  ClearResource();

  ScriptResource* script_resource = ToScriptResource(resource);

  HeapVector<Member<ConsoleMessage>> error_messages;
  if (!WasModuleLoadSuccessful(script_resource, &error_messages)) {
    client_->NotifyFetchFinished(base::nullopt, error_messages);
    return;
  }

  ModuleScriptCreationParams params(
      script_resource->GetResponse().Url(), script_resource->SourceText(),
      script_resource->GetResourceRequest().GetFetchCredentialsMode(),
      script_resource->CalculateAccessControlStatus(
          fetcher_->Context().GetSecurityOrigin()));
  client_->NotifyFetchFinished(params, error_messages);
}

void DocumentModuleScriptFetcher::Trace(blink::Visitor* visitor) {
  visitor->Trace(fetcher_);
  visitor->Trace(client_);
  ResourceClient::Trace(visitor);
}

bool DocumentModuleScriptFetcher::FetchIfLayeredAPI(
    FetchParameters& fetch_params) {
  if (!RuntimeEnabledFeatures::LayeredAPIEnabled())
    return false;

  KURL layered_api_url = blink::layered_api::GetInternalURL(fetch_params.Url());

  if (layered_api_url.IsNull())
    return false;

  const String source_text = blink::layered_api::GetSourceText(layered_api_url);

  if (source_text.IsNull()) {
    HeapVector<Member<ConsoleMessage>> error_messages;
    error_messages.push_back(ConsoleMessage::CreateForRequest(
        kJSMessageSource, kErrorMessageLevel, "Unexpected data error",
        fetch_params.Url().GetString(), nullptr, 0));
    client_->NotifyFetchFinished(base::nullopt, error_messages);
    return true;
  }

  ModuleScriptCreationParams params(
      layered_api_url, source_text,
      fetch_params.GetResourceRequest().GetFetchCredentialsMode(),
      kSharableCrossOrigin);
  client_->NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
  return true;
}

}  // namespace blink
