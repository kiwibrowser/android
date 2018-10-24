// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_infobar_delegate.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void RecordInteractionUMA(
    PageLoadCappingInfoBarDelegate::InfoBarInteraction interaction) {
  UMA_HISTOGRAM_ENUMERATION("HeavyPageCapping.InfoBarInteraction", interaction);
}

// The infobar that allows the user to resume resource loading on the page.
class ResumeDelegate : public PageLoadCappingInfoBarDelegate {
 public:
  // |pause_callback| will either pause subresource loading or resume it based
  // on the passed in bool.
  explicit ResumeDelegate(const PauseCallback& pause_callback)
      : pause_callback_(pause_callback) {}
  ~ResumeDelegate() override = default;

 private:
  // PageLoadCappingInfoBarDelegate:
  base::string16 GetMessageText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_STOPPED_TITLE);
  }
  base::string16 GetLinkText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_CONTINUE_MESSAGE);
  }
  bool LinkClicked(WindowOpenDisposition disposition) override {
    RecordInteractionUMA(InfoBarInteraction::kResumedPage);
    if (pause_callback_.is_null())
      return true;
    // Pass false to resume subresource loading.
    pause_callback_.Run(false);
    return true;
  }

  // |pause_callback| will either pause subresource loading or resume it based
  // on the passed in bool.
  PauseCallback pause_callback_;

  DISALLOW_COPY_AND_ASSIGN(ResumeDelegate);
};

// The infobar that allows the user to pause resoruce loading on the page.
class PauseDelegate : public PageLoadCappingInfoBarDelegate {
 public:
  // This object is destroyed wqhen the page is terminated, and methods related
  // to functionality of the infobar (E.g., LinkClicked()), are not called from
  // page destructors. This object is also destroyed on all non-same page
  // navigations.
  // |pause_callback| is a callback that will pause subresource loading on the
  // page.
  PauseDelegate(int64_t bytes_threshold, const PauseCallback& pause_callback)
      : bytes_threshold_(bytes_threshold), pause_callback_(pause_callback) {}
  ~PauseDelegate() override = default;

 private:
  // PageLoadCappingInfoBarDelegate:
  base::string16 GetMessageText() const override {
    return l10n_util::GetStringFUTF16Int(
        IDS_PAGE_CAPPING_TITLE,
        static_cast<int>(bytes_threshold_ / 1024 / 1024));
  }

  base::string16 GetLinkText() const override {
    return l10n_util::GetStringUTF16(IDS_PAGE_CAPPING_STOP_MESSAGE);
  }

  bool LinkClicked(WindowOpenDisposition disposition) override {
    RecordInteractionUMA(InfoBarInteraction::kPausedPage);
    if (!pause_callback_.is_null()) {
      // Pause subresouce loading on the page.
      pause_callback_.Run(true);
    }

    auto* infobar_manager = infobar()->owner();
    // |this| will be gone after this call.
    infobar_manager->ReplaceInfoBar(
        infobar(), infobar_manager->CreateConfirmInfoBar(
                       std::make_unique<ResumeDelegate>(pause_callback_)));

    return false;
  }

 private:
  // The amount of bytes that was exceeded to trigger this infobar.
  int64_t bytes_threshold_;

  // |pause_callback| will either pause subresource loading or resume it based
  // on the passed in bool.
  PauseCallback pause_callback_;

  DISALLOW_COPY_AND_ASSIGN(PauseDelegate);
};

}  // namespace

// static
bool PageLoadCappingInfoBarDelegate::Create(
    int64_t bytes_threshold,
    content::WebContents* web_contents,
    const PauseCallback& pause_callback) {
  auto* infobar_service = InfoBarService::FromWebContents(web_contents);
  RecordInteractionUMA(InfoBarInteraction::kShowedInfoBar);
  // WrapUnique is used to allow for a private constructor.
  return infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<PauseDelegate>(bytes_threshold, pause_callback)));
}

PageLoadCappingInfoBarDelegate::~PageLoadCappingInfoBarDelegate() = default;

PageLoadCappingInfoBarDelegate::PageLoadCappingInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
PageLoadCappingInfoBarDelegate::GetIdentifier() const {
  return PAGE_LOAD_CAPPING_INFOBAR_DELEGATE;
}

int PageLoadCappingInfoBarDelegate::GetIconId() const {
// TODO(ryansturm): Make data saver resources available on other platforms.
// https://crbug.com/820594
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_PREVIEWS;
#else
  return kNoIconID;
#endif
}

bool PageLoadCappingInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return details.is_navigation_to_different_page;
}

int PageLoadCappingInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
