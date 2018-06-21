// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_capping_page_load_metrics_observer.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/data_use_measurement/page_load_capping/chrome_page_load_capping_features.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_infobar_delegate.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

const char kMediaPageCap[] = "MediaPageCapMiB";
const char kPageCap[] = "PageCapMiB";

const char kMediaPageTypical[] = "MediaPageTypicalLargePageMiB";
const char kPageTypical[] = "PageTypicalLargePageMiB";

const char kPageFuzzing[] = "PageFuzzingKiB";

// The page load capping bytes threshold for the page. There are seperate
// thresholds for media and non-media pages. Returns empty optional if the
// page should not be capped.
base::Optional<int64_t> GetPageLoadCappingBytesThreshold(bool media_page_load) {
  if (!base::FeatureList::IsEnabled(data_use_measurement::page_load_capping::
                                        features::kDetectingHeavyPages)) {
    return base::nullopt;
  }
  // Defaults are 15 MiB for media and 5 MiB for non-media.
  int64_t default_cap_mib = media_page_load ? 15 : 5;
  return base::GetFieldTrialParamByFeatureAsInt(
             data_use_measurement::page_load_capping::features::
                 kDetectingHeavyPages,
             (media_page_load ? kMediaPageCap : kPageCap), default_cap_mib) *
         1024 * 1024;
}

// Provides an estimate of savings based on the typical size of page loads above
// the capping thresholds.
int64_t GetEstimatedSavings(int64_t network_bytes,
                            int64_t threshold,
                            bool media_page_load) {
  // These are estimated by the median size page above the capping threshold
  int64_t typical_size =
      base::GetFieldTrialParamByFeatureAsInt(
          data_use_measurement::page_load_capping::features::
              kDetectingHeavyPages,
          (media_page_load ? kMediaPageTypical : kPageTypical), 0) *
      1024 * 1024;
  if (typical_size == 0) {
    // Defaults are capping thresholds inflated 50 percent.
    typical_size = threshold * 1.5;
  }
  // If this page load already exceeded the typical page load size, report 0
  // savings.
  return std::max<int64_t>((typical_size - network_bytes), 0);
}

}  // namespace

PageCappingPageLoadMetricsObserver::PageCappingPageLoadMetricsObserver()
    : weak_factory_(this) {}
PageCappingPageLoadMetricsObserver::~PageCappingPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageCappingPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  web_contents_ = navigation_handle->GetWebContents();
  page_cap_ = GetPageLoadCappingBytesThreshold(false /* media_page_load */);
  url_host_ = navigation_handle->GetURL().host();
  fuzzing_offset_ = GetFuzzingOffset();

  MaybeCreate();
  // TODO(ryansturm) Check a blacklist of eligible pages.
  // https://crbug.com/797981
  return page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING;
}

void PageCappingPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.was_cached)
    return;
  network_bytes_ += extra_request_complete_info.raw_body_bytes;
  MaybeCreate();
}

void PageCappingPageLoadMetricsObserver::MaybeCreate() {
  // If the infobar has already been shown for the page, don't show an infobar.
  if (page_capping_state_ != PageCappingState::kInfoBarNotShown)
    return;

  // If the page has not committed, don't show an infobar.
  if (!web_contents_)
    return;

  // If there is no capping threshold, the threshold or the threshold is not
  // met, do not show an infobar. Use the fuzzing offset to increase the number
  // of bytes needed.
  if (!page_cap_ || (network_bytes_ - fuzzing_offset_) < page_cap_.value())
    return;

  if (PageLoadCappingInfoBarDelegate::Create(
          page_cap_.value(), web_contents_,
          base::BindRepeating(
              &PageCappingPageLoadMetricsObserver::PauseSubresourceLoading,
              weak_factory_.GetWeakPtr()))) {
    page_capping_state_ = PageCappingState::kInfoBarShown;
  }
}

void PageCappingPageLoadMetricsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    bool is_in_main_frame) {
  media_page_load_ = true;
  page_cap_ = GetPageLoadCappingBytesThreshold(true /* media_page_load */);
}

void PageCappingPageLoadMetricsObserver::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the page is not paused, there is no need to pause new frames.
  if (page_capping_state_ != PageCappingState::kPagePaused)
    return;
  // If the navigation is to the same page, is to an error page, the load hasn't
  // committed or render frame host is null, no need to pause the page.
  if (navigation_handle->IsSameDocument() || navigation_handle->IsErrorPage() ||
      !navigation_handle->HasCommitted() ||
      !navigation_handle->GetRenderFrameHost()) {
    return;
  }
  // Pause the new frame.
  handles_.push_back(
      navigation_handle->GetRenderFrameHost()->PauseSubresourceLoading());
}

void PageCappingPageLoadMetricsObserver::PauseSubresourceLoading(bool pause) {
  DCHECK((pause && page_capping_state_ == PageCappingState::kInfoBarShown) ||
         (!pause && page_capping_state_ == PageCappingState::kPagePaused));
  page_capping_state_ =
      pause ? PageCappingState::kPagePaused : PageCappingState::kPageResumed;
  if (pause)
    handles_ = web_contents_->PauseSubresourceLoading();
  else
    handles_.clear();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageCappingPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordDataSavingsAndUKM(info);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PageCappingPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordDataSavingsAndUKM(info);
  return CONTINUE_OBSERVING;
}

void PageCappingPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordDataSavingsAndUKM(info);
  if (page_capping_state_ == PageCappingState::kPagePaused) {
    PAGE_BYTES_HISTOGRAM("HeavyPageCapping.RecordedDataSavings",
                         recorded_savings_);
  }
}

void PageCappingPageLoadMetricsObserver::RecordDataSavingsAndUKM(
    const page_load_metrics::PageLoadExtraInfo& info) {
  // If the InfoBar was never shown, don't report savings or UKM.
  if (page_capping_state_ == PageCappingState::kInfoBarNotShown) {
    DCHECK_EQ(0, recorded_savings_);
    return;
  }

  if (!ukm_recorded_) {
    ukm::builders::PageLoadCapping builder(info.source_id);
    builder.SetFinalState(static_cast<int64_t>(page_capping_state_));
    builder.Record(ukm::UkmRecorder::Get());
    ukm_recorded_ = true;
  }
  // If the InfoBar was shown, but not acted upon, don't update savings.
  if (page_capping_state_ == PageCappingState::kInfoBarShown) {
    DCHECK_EQ(0, recorded_savings_);
    return;
  }

  // If the user resumed, we may need to update the savings.
  if (page_capping_state_ == PageCappingState::kPageResumed) {
    // No need to undo savings if no savings were previously recorded.
    if (recorded_savings_ == 0)
      return;
    // Undo previous savings since the page was resumed.
    WriteToSavings(-1 * recorded_savings_);
    recorded_savings_ = 0;
    return;
  }

  DCHECK_EQ(PageCappingState::kPagePaused, page_capping_state_);

  int64_t estimated_savings =
      GetEstimatedSavings(network_bytes_, page_cap_.value(), media_page_load_);
  // Record an update to the savings. |recorded_savings_| is generally larger
  // than |estimated_savings| when called a second time.
  WriteToSavings(estimated_savings - recorded_savings_);

  recorded_savings_ = estimated_savings;
}

void PageCappingPageLoadMetricsObserver::WriteToSavings(int64_t bytes_saved) {
  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              web_contents_->GetBrowserContext());

  bool data_saver_enabled =
      data_reduction_proxy_settings->IsDataReductionProxyEnabled();

  data_reduction_proxy_settings->data_reduction_proxy_service()
      ->UpdateDataUseForHost(0, bytes_saved, url_host_);

  data_reduction_proxy_settings->data_reduction_proxy_service()
      ->UpdateContentLengths(0, bytes_saved, data_saver_enabled,
                             data_reduction_proxy::HTTPS, "text/html");
}

int64_t PageCappingPageLoadMetricsObserver::GetFuzzingOffset() const {
  if (!base::FeatureList::IsEnabled(data_use_measurement::page_load_capping::
                                        features::kDetectingHeavyPages)) {
    return 0;
  }
  // Default is is 75 KiB.
  int cap_kib = 75;

  cap_kib = base::GetFieldTrialParamByFeatureAsInt(
      data_use_measurement::page_load_capping::features::kDetectingHeavyPages,
      kPageFuzzing, cap_kib);

  int cap_bytes = cap_kib * 1024;

  return base::RandInt(0, cap_bytes);
}

bool PageCappingPageLoadMetricsObserver::IsPausedForTesting() const {
  return page_capping_state_ == PageCappingState::kPagePaused;
}
