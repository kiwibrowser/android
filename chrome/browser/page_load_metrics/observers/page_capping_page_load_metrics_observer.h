// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_CAPPING_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_CAPPING_PAGE_LOAD_METRICS_OBSERVER_H_

#include <vector>

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"

namespace content {
class WebContents;
}  // namespace content

// A class that tracks the data usage of a page load and triggers an infobar
// when the page load is above a certain threshold. The thresholds are field
// trial controlled and vary based on whether media has played on the page.
// TODO(ryansturm): This class can change the functionality of the page itself
// through pausing subresource loading (by owning a collection of
// PauseSubResourceLoadingHandlePtr's). This type of behavior is typically not
// seen in page load metrics observers, but the PageLoadTracker functionality
// (request data usage) is necessary for determining triggering conditions.
// Consider moving to a WebContentsObserver/TabHelper and source byte updates
// from this class to that observer. https://crbug.com/840399
class PageCappingPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PageCappingPageLoadMetricsObserver();
  ~PageCappingPageLoadMetricsObserver() override;

  // Returns whether the page's subresource loading is currently paused.
  bool IsPausedForTesting() const;

  // The current state of the page.
  // This class operates as a state machine going from each of the below states
  // in order. This is recorded to UKM, so the enum should not be changed.
  enum class PageCappingState {
    // The initial state of the page. No InfoBar has been shown.
    kInfoBarNotShown = 0,
    // When the cap is met, an InfoBar will be shown.
    kInfoBarShown = 1,
    // If the user clicks pause on the InfoBar, the page will be paused.
    kPagePaused = 2,
    // If the user then clicks resume on the InfoBar the page is resumed. This
    // is the final state.
    kPageResumed = 3,
  };

 protected:
  // Virtual for testing.
  // Gets the random offset for the capping threshold.
  virtual int64_t GetFuzzingOffset() const;

 private:
  // page_load_metrics::PageLoadMetricsObserver:
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      bool is_in_main_frame) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadExtraInfo& info) override;
  void OnComplete(const page_load_metrics::mojom::PageLoadTiming& timing,
                  const page_load_metrics::PageLoadExtraInfo& info) override;

  // Records a new estimate of data savings based on data used and field trial
  // params. Also records the PageCappingState to UKM.
  void RecordDataSavingsAndUKM(
      const page_load_metrics::PageLoadExtraInfo& info);

  // Writes the amount of savings to the data saver feature. Virtual for
  // testing.
  virtual void WriteToSavings(int64_t bytes_saved);

  // Show the page capping infobar if it has not been shown before and the data
  // use is above the threshold.
  void MaybeCreate();

  // Pauses or unpauses the subresource loading of the page based on |paused|.
  // TODO(ryansturm): New Subframes will not be paused automatically and may
  // load resources. https://crbug.com/835895
  void PauseSubresourceLoading(bool paused);

  // The current bytes threshold of the capping page triggering.
  base::Optional<int64_t> page_cap_;

  // The WebContents for this page load. |this| cannot outlive |web_contents|.
  content::WebContents* web_contents_ = nullptr;

  // The host to attribute savings to.
  std::string url_host_;

  // Whether a media element has been played on the page.
  bool media_page_load_ = false;

  // The cumulative network body bytes used so far.
  int64_t network_bytes_ = 0;

  // The amount of bytes when the data savings was last recorded.
  int64_t recorded_savings_ = 0;

  PageCappingState page_capping_state_ = PageCappingState::kInfoBarNotShown;

  // True once UKM has been recorded. This is recorded at the same time as
  // PageLoad UKM (during hidden, complete, or app background).
  bool ukm_recorded_ = false;

  // The randomly generated offset from the capping threshold.
  int64_t fuzzing_offset_ = 0;

  // If non-empty, a group of handles that are pausing subresource loads in the
  // render frames of this page.
  std::vector<blink::mojom::PauseSubresourceLoadingHandlePtr> handles_;

  base::WeakPtrFactory<PageCappingPageLoadMetricsObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageCappingPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_PAGE_CAPPING_PAGE_LOAD_METRICS_OBSERVER_H_
