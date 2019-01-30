// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetcher.h"

#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void ModuleScriptFetcher::Client::OnFetched(
    const base::Optional<ModuleScriptCreationParams>& params) {
  NotifyFetchFinished(params, HeapVector<Member<ConsoleMessage>>());
}

void ModuleScriptFetcher::Client::OnFailed() {
  NotifyFetchFinished(base::nullopt, HeapVector<Member<ConsoleMessage>>());
}

bool ModuleScriptFetcher::WasModuleLoadSuccessful(
    Resource* resource,
    HeapVector<Member<ConsoleMessage>>* error_messages) {
  // Implements conditions in Step 7 of
  // https://html.spec.whatwg.org/#fetch-a-single-module-script

  DCHECK(error_messages);

  if (resource) {
    SubresourceIntegrityHelper::GetConsoleMessages(
        resource->IntegrityReportInfo(), error_messages);
  }

  // - response's type is "error"
  if (!resource || resource->ErrorOccurred() ||
      resource->IntegrityDisposition() !=
          ResourceIntegrityDisposition::kPassed) {
    return false;
  }

  const auto& response = resource->GetResponse();
  // - response's status is not an ok status
  if (response.IsHTTP() && !CORS::IsOkStatus(response.HttpStatusCode())) {
    return false;
  }

  // The result of extracting a MIME type from response's header list
  // (ignoring parameters) is not a JavaScript MIME type
  // Note: For historical reasons, fetching a classic script does not include
  // MIME type checking. In contrast, module scripts will fail to load if they
  // are not of a correct MIME type.
  // We use ResourceResponse::HttpContentType() instead of MimeType(), as
  // MimeType() may be rewritten by mime sniffer.
  if (!MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
          response.HttpContentType())) {
    String message =
        "Failed to load module script: The server responded with a "
        "non-JavaScript MIME type of \"" +
        response.HttpContentType() +
        "\". Strict MIME type checking is enforced for module scripts per "
        "HTML spec.";
    error_messages->push_back(ConsoleMessage::CreateForRequest(
        kJSMessageSource, kErrorMessageLevel, message,
        response.Url().GetString(), nullptr, resource->Identifier()));
    return false;
  }

  return true;
}

}  // namespace blink
