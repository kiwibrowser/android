// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/performance_measurement_manager.h"

#include "chrome/browser/resource_coordinator/render_process_probe.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"

namespace resource_coordinator {

using LoadingState = TabLoadTracker::LoadingState;

PerformanceMeasurementManager::PerformanceMeasurementManager(
    TabLoadTracker* tab_load_tracker,
    RenderProcessProbe* render_process_probe)
    : scoped_observer_(this), render_process_probe_(render_process_probe) {
  scoped_observer_.Add(tab_load_tracker);
}

PerformanceMeasurementManager::~PerformanceMeasurementManager() = default;

void PerformanceMeasurementManager::OnStartTracking(
    content::WebContents* web_contents,
    LoadingState loading_state) {
  if (loading_state == LoadingState::LOADED)
    render_process_probe_->StartSingleGather();
}

void PerformanceMeasurementManager::OnLoadingStateChange(
    content::WebContents* web_contents,
    LoadingState old_loading_state,
    LoadingState new_loading_state) {
  if (new_loading_state == LoadingState::LOADED)
    render_process_probe_->StartSingleGather();
}

}  // namespace resource_coordinator
