// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BloatedRendererTabHelper);

BloatedRendererTabHelper::BloatedRendererTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {}

void BloatedRendererTabHelper::WillReloadPageWithBloatedRenderer() {
  reloading_bloated_renderer_ = true;
}

void BloatedRendererTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(ulan): Use nagivation_handle to ensure that the finished navigation
  // is the same nagivation started by reloading the bloated tab.
  if (reloading_bloated_renderer_) {
    ShowInfoBar(InfoBarService::FromWebContents(web_contents()));
    reloading_bloated_renderer_ = false;
  }
}

void BloatedRendererTabHelper::ShowInfoBar(InfoBarService* infobar_service) {
  if (!infobar_service) {
    // No infobar service in unit-tests.
    return;
  }
  SimpleAlertInfoBarDelegate::Create(
      infobar_service,
      infobars::InfoBarDelegate::BLOATED_RENDERER_INFOBAR_DELEGATE, nullptr,
      l10n_util::GetStringUTF16(IDS_BROWSER_BLOATED_RENDERER_INFOBAR), false);
}
