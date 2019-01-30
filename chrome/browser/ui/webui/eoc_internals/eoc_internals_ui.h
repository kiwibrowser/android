// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EOC_INTERNALS_EOC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_EOC_INTERNALS_EOC_INTERNALS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/eoc_internals/eoc_internals.mojom.h"
#include "chrome/browser/ui/webui/eoc_internals/eoc_internals_page_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"

// UI controller for chrome://eoc-internals, hooks up a concrete implementation
// of eoc_internals::mojom::PageHandler to requests for that page handler
// that will come from the frontend.
class EocInternalsUI : public ui::MojoWebUIController {
 public:
  explicit EocInternalsUI(content::WebUI* web_ui);
  ~EocInternalsUI() override;

 private:
  void BindEocInternalsPageHandler(
      eoc_internals::mojom::PageHandlerRequest request);

  std::unique_ptr<EocInternalsPageHandler> page_handler_;
  contextual_suggestions::ContextualContentSuggestionsService*
      contextual_content_suggestions_service_;

  DISALLOW_COPY_AND_ASSIGN(EocInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EOC_INTERNALS_EOC_INTERNALS_UI_H_
