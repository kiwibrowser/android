// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_FILTER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_FILTER_H_

#include "base/macros.h"

namespace content {

// This class is used to send a signal to ResourceScheduler. This class used
// to be a ResourceMessageFilter, but is not any more.
class ResourceSchedulerFilter {
 public:
  // Informs the ResourceScheduler that a main-frame, non-same-document
  // navigation has just committed.
  static void OnDidCommitMainframeNavigation(int render_process_id,
                                             int render_view_routing_id);
 private:
  ResourceSchedulerFilter() = delete;
  ~ResourceSchedulerFilter() = delete;

  DISALLOW_COPY_AND_ASSIGN(ResourceSchedulerFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_FILTER_H_
