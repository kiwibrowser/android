// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_WEBCONTENTS_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_WEBCONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_writer.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

namespace content {
class NavigationHandle;
}

namespace resource_coordinator {

// WebContents observer that manages a SiteCharacteristicsDataWriter associated
// with a WebContents and forwards the appropriate events to it.
class LocalSiteCharacteristicsWebContentsObserver
    : public content::WebContentsObserver,
      public TabLoadTracker::Observer,
      public PageSignalObserver {
 public:
  // Allows skipping the observer registration in the constructor for the
  // unittests that don't have all the services enabled
  // (e.g. PageSignalReceiver), these features should be tested in integration
  // tests instead.
  static void SkipObserverRegistrationForTesting();

  explicit LocalSiteCharacteristicsWebContentsObserver(
      content::WebContents* contents);
  ~LocalSiteCharacteristicsWebContentsObserver() override;

  // WebContentsObserver implementation.
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  void OnAudioStateChanged(bool audible) override;

  // TabLoadTracker::Observer:
  void OnLoadingStateChange(content::WebContents* web_contents,
                            LoadingState old_loading_state,
                            LoadingState new_loading_state) override;

  // PageSignalObserver:
  void OnNonPersistentNotificationCreated(
      content::WebContents* web_contents) override;

  SiteCharacteristicsDataWriter* GetWriterForTesting() const {
    return writer_.get();
  }
  url::Origin GetWriterOriginForTesting() const { return writer_origin_; }

 private:
  // Indicates if the feature usage event just received should be ignored.
  bool ShouldIgnoreFeatureUsageEvent();

  // Helper function to maybe notify |writer_| that a feature event has been
  // received while in background. Doen't do anything if
  // ShouldIgnoreFeatureUsageEvent returns true or if the tab isn't
  // backgrounded.
  void MaybeNotifyBackgroundFeatureUsage(
      void (SiteCharacteristicsDataWriter::*method)());

  // The writer that processes the event received by this class.
  std::unique_ptr<SiteCharacteristicsDataWriter> writer_;

  // The Origin tracked by the writer.
  url::Origin writer_origin_;

  // Favicon and title are set when a page is loaded, we only want to send
  // signals to the database about title and favicon update from the previous
  // title and favicon, thus we want to ignore the very first update since it is
  // always supposed to happen.
  bool first_time_favicon_set_ = false;
  bool first_time_title_set_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsWebContentsObserver);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LOCAL_SITE_CHARACTERISTICS_WEBCONTENTS_OBSERVER_H_
