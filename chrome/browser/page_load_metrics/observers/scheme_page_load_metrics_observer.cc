// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/scheme_page_load_metrics_observer.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  return started_in_foreground ? CONTINUE_OBSERVING : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  if (navigation_handle->GetURL().scheme() == url::kHttpScheme ||
      navigation_handle->GetURL().scheme() == url::kHttpsScheme) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SchemePageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  return STOP_OBSERVING;
}

void SchemePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (extra_info.url.scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.ParseTiming.NavigationToParseStart",
        timing.parse_timing->parse_start.value());
  } else if (extra_info.url.scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.ParseTiming.NavigationToParseStart",
        timing.parse_timing->parse_start.value());
  }
}

void SchemePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (extra_info.url.scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.PaintTiming."
        "NavigationToFirstContentfulPaint",
        timing.paint_timing->first_contentful_paint.value());
  } else if (extra_info.url.scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.PaintTiming."
        "NavigationToFirstContentfulPaint",
        timing.paint_timing->first_contentful_paint.value());
  }
}

void SchemePageLoadMetricsObserver::OnFirstMeaningfulPaintInMainFrameDocument(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (extra_info.url.scheme() == url::kHttpScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTP.Experimental.PaintTiming."
        "NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  } else if (extra_info.url.scheme() == url::kHttpsScheme) {
    PAGE_LOAD_HISTOGRAM(
        "PageLoad.Clients.Scheme.HTTPS.Experimental.PaintTiming."
        "NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  }
}
