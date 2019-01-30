// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_FAVICON_FAVICON_LOADER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
@class FaviconAttributes;

namespace favicon {
class LargeIconService;
}

// A class that manages asynchronously loading favicons or fallback attributes
// from LargeIconService and caching them, given a URL.
class FaviconLoader : public KeyedService {
 public:
  // Type for completion block for FaviconForURL().
  typedef void (^FaviconAttributesCompletionBlock)(FaviconAttributes*);

  explicit FaviconLoader(favicon::LargeIconService* large_icon_service);
  ~FaviconLoader() override;

  // Returns the FaviconAttributes for the favicon retrieved from |url|.
  // If no icon is in favicon_cache_, will start an asynchronous load with the
  // favicon service and return the default favicon.
  // Calls |block| when the load completes with a valid image/attributes.
  // Note: If no successful favicon was retrieved by LargeIconService, it
  // returns the default favicon.
  FaviconAttributes* FaviconForUrl(const GURL& url,
                                   float size,
                                   float min_size,
                                   FaviconAttributesCompletionBlock block);

  // Cancel all incomplete requests.
  void CancellAllRequests();

 private:
  // The LargeIconService used to retrieve favicon.
  favicon::LargeIconService* large_icon_service_;

  // Tracks tasks sent to FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;
  // Holds cached favicons. This NSCache is populated as favicons or fallback
  // attributes are retrieved from the FaviconService. Contents will be removed
  // during low-memory conditions based on its inherent LRU removal algorithm.
  // Keyed by NSString of URL spec.
  NSCache<NSString*, FaviconAttributes*>* favicon_cache_;

  DISALLOW_COPY_AND_ASSIGN(FaviconLoader);
};

#endif  // IOS_CHROME_BROWSER_FAVICON_FAVICON_LOADER_H_
