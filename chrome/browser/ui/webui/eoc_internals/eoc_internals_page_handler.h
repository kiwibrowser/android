// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EOC_INTERNALS_EOC_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EOC_INTERNALS_EOC_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/eoc_internals/eoc_internals.mojom.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"
#include "mojo/public/cpp/bindings/binding.h"

// Concrete implementation of eoc_internals::mojom::PageHandler.
class EocInternalsPageHandler : public eoc_internals::mojom::PageHandler {
 public:
  EocInternalsPageHandler(
      eoc_internals::mojom::PageHandlerRequest request,
      contextual_suggestions::ContextualContentSuggestionsService*
          contextual_content_suggestions_service);
  ~EocInternalsPageHandler() override;

 private:
  // eoc_internals::mojom::EocInternalsPageHandler
  void GetProperties(GetPropertiesCallback) override;
  void SetTriggerTime(int64_t seconds) override;
  void GetCachedMetricEvents(GetCachedMetricEventsCallback) override;
  void ClearCachedMetricEvents(ClearCachedMetricEventsCallback) override;
  void GetCachedSuggestionResults(GetCachedSuggestionResultsCallback) override;
  void ClearCachedSuggestionResults(
      ClearCachedSuggestionResultsCallback) override;

  mojo::Binding<eoc_internals::mojom::PageHandler> binding_;
  contextual_suggestions::ContextualContentSuggestionsService*
      contextual_content_suggestions_service_;

  DISALLOW_COPY_AND_ASSIGN(EocInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EOC_INTERNALS_EOC_INTERNALS_PAGE_HANDLER_H_
