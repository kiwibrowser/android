// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_scheduler_filter.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "services/network/resource_scheduler.h"

namespace content {

// Some tests are lacking a ResourceDispatcherHostImpl.
network::ResourceScheduler* GetResourceSchedulerOrNullptr() {
  if (!ResourceDispatcherHostImpl::Get())
    return nullptr;
  return ResourceDispatcherHostImpl::Get()->scheduler();
}

// static
void ResourceSchedulerFilter::OnDidCommitMainframeNavigation(
    int render_process_id,
    int render_view_routing_id) {
  auto* scheduler = GetResourceSchedulerOrNullptr();
  if (scheduler)
    scheduler->DeprecatedOnNavigate(render_process_id, render_view_routing_id);
}

}  // namespace content
