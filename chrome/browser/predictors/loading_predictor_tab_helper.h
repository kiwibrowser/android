// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace predictors {

class LoadingPredictor;

// Observes various page load events from the navigation start to onload
// completed and notifies the LoadingPredictor associated with the current
// profile.
//
// All methods must be called from the UI thread.
class LoadingPredictorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<LoadingPredictorTabHelper> {
 public:
  ~LoadingPredictorTabHelper() override;

  // content::WebContentsObserver implementation
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::mojom::ResourceLoadInfo& resource_load_info) override;
  void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& mime_type,
      content::ResourceType resource_type) override;
  void DocumentOnLoadCompletedInMainFrame() override;

  void SetLoadingPredictorForTesting(
      base::WeakPtr<LoadingPredictor> predictor) {
    predictor_ = predictor;
  }

 private:
  explicit LoadingPredictorTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<LoadingPredictorTabHelper>;

  // Owned by profile.
  base::WeakPtr<LoadingPredictor> predictor_;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorTabHelper);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_TAB_HELPER_H_
