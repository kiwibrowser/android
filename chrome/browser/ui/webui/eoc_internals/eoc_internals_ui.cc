// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/eoc_internals/eoc_internals_ui.h"

#include "build/build_config.h"
#include "chrome/browser/ntp_snippets/contextual_content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/eoc_internals/eoc_internals_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif

using contextual_suggestions::ContextualContentSuggestionsService;

EocInternalsUI::EocInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIEocInternalsHost);

  source->AddResourcePath("eoc_internals.css", IDR_EOC_INTERNALS_CSS);
  source->AddResourcePath("eoc_internals.js", IDR_EOC_INTERNALS_JS);
  source->AddResourcePath("eoc_internals.mojom.js", IDR_EOC_INTERNALS_MOJO_JS);
  source->SetDefaultResource(IDR_EOC_INTERNALS_HTML);
  source->UseGzip();

  Profile* profile = Profile::FromWebUI(web_ui);
  contextual_content_suggestions_service_ =
      ContextualContentSuggestionsServiceFactory::GetForProfile(profile);
  content::WebUIDataSource::Add(profile, source);
  // This class is the caller of the callback when an observer interface is
  // triggered. So this base::Unretained is safe.
  AddHandlerToRegistry(base::BindRepeating(
      &EocInternalsUI::BindEocInternalsPageHandler, base::Unretained(this)));
}

EocInternalsUI::~EocInternalsUI() {}

void EocInternalsUI::BindEocInternalsPageHandler(
    eoc_internals::mojom::PageHandlerRequest request) {
  page_handler_.reset(new EocInternalsPageHandler(
      std::move(request), contextual_content_suggestions_service_));
}
