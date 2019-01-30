// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_PERFORMANCE_MEASUREMENT_MANAGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_PERFORMANCE_MEASUREMENT_MANAGER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

namespace resource_coordinator {

class RenderProcessProbe;
class TabLoadTracker;

// This class observes tab state change notifications issued by the
// TabLoadTracker and uses them to drive performance measurement requests
// to the RenderProcessProbe. Results then funnel through the resource
// coordinator service, back to this class, which stores them in the feature
// database.
class PerformanceMeasurementManager : public TabLoadTracker::Observer {
 public:
  PerformanceMeasurementManager(TabLoadTracker* tab_load_tracker,
                                RenderProcessProbe* render_process_probe);
  ~PerformanceMeasurementManager() override;

  // TabLoadTracker::Observer implementation.
  void OnStartTracking(content::WebContents* web_contents,
                       LoadingState loading_state) override;
  void OnLoadingStateChange(content::WebContents* web_contents,
                            LoadingState old_loading_state,
                            LoadingState new_loading_state) override;

 private:
  ScopedObserver<TabLoadTracker, PerformanceMeasurementManager>
      scoped_observer_;
  RenderProcessProbe* render_process_probe_;

  DISALLOW_COPY_AND_ASSIGN(PerformanceMeasurementManager);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_PERFORMANCE_MEASUREMENT_MANAGER_H_
