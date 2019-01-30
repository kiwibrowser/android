// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/session_restore_policy.h"

#include "base/no_destructor.h"
#include "base/sys_info.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

namespace {

class SysInfoDelegate : public SessionRestorePolicy::Delegate {
 public:
  SysInfoDelegate() {}
  ~SysInfoDelegate() override {}

  size_t GetNumberOfCores() const override {
    return base::SysInfo::NumberOfProcessors();
  }

  size_t GetFreeMemoryMiB() const override {
    constexpr int64_t kMibibytesInBytes = 1 << 20;
    int64_t free_mem =
        base::SysInfo::AmountOfAvailablePhysicalMemory() / kMibibytesInBytes;
    DCHECK(free_mem >= 0);
    return free_mem;
  }

  base::TimeTicks NowTicks() const override { return base::TimeTicks::Now(); }

  size_t GetSiteEngagementScore(content::WebContents* contents) const override {
    // Get the active navigation entry. Restored tabs should always have one.
    auto& controller = contents->GetController();
    auto* nav_entry =
        controller.GetEntryAtIndex(controller.GetCurrentEntryIndex());
    DCHECK(nav_entry);

    auto* engagement_svc = SiteEngagementService::Get(
        Profile::FromBrowserContext(contents->GetBrowserContext()));
    double engagement =
        engagement_svc->GetDetails(nav_entry->GetURL()).total_score;

    // Return the engagement as an integer.
    return engagement;
  }

  static SysInfoDelegate* Get() {
    static base::NoDestructor<SysInfoDelegate> delegate;
    return delegate.get();
  }
};

}  // namespace

SessionRestorePolicy::SessionRestorePolicy()
    : policy_enabled_(
          base::FeatureList::IsEnabled(features::kInfiniteSessionRestore)),
      delegate_(SysInfoDelegate::Get()),
      parsed_params_(GetInfiniteSessionRestoreParams()),
      params_(&parsed_params_),
      simultaneous_tab_loads_(CalculateSimultaneousTabLoadsFromParams()) {}

SessionRestorePolicy::~SessionRestorePolicy() = default;

bool SessionRestorePolicy::ShouldLoad(content::WebContents* contents) const {
  // If the policy is disabled then always return true.
  if (!policy_enabled_)
    return true;

  if (tab_loads_started_ < params_->min_tabs_to_restore)
    return true;

  if (params_->max_tabs_to_restore != 0 &&
      tab_loads_started_ >= params_->max_tabs_to_restore) {
    return false;
  }

  // If there is a free memory constraint then enforce it.
  if (params_->mb_free_memory_per_tab_to_restore != 0) {
    size_t free_mem_mb = delegate_->GetFreeMemoryMiB();
    if (free_mem_mb < params_->mb_free_memory_per_tab_to_restore)
      return false;
  }

  // Enforce a max time since use if one is specified.
  if (!params_->max_time_since_last_use_to_restore.is_zero()) {
    base::TimeDelta time_since_active =
        delegate_->NowTicks() - contents->GetLastActiveTime();
    if (time_since_active > params_->max_time_since_last_use_to_restore)
      return false;
  }

  // Enforce a minimum site engagement score.
  if (delegate_->GetSiteEngagementScore(contents) <
      params_->min_site_engagement_to_restore) {
    return false;
  }

  return true;
}

void SessionRestorePolicy::NotifyTabLoadStarted() {
  ++tab_loads_started_;
}

SessionRestorePolicy::SessionRestorePolicy(
    bool policy_enabled,
    const Delegate* delegate,
    const InfiniteSessionRestoreParams* params)
    : policy_enabled_(policy_enabled),
      delegate_(delegate),
      params_(params),
      simultaneous_tab_loads_(CalculateSimultaneousTabLoadsFromParams()) {}

// static
size_t SessionRestorePolicy::CalculateSimultaneousTabLoads(
    size_t min_loads,
    size_t max_loads,
    size_t cores_per_load,
    size_t num_cores) {
  DCHECK(max_loads == 0 || min_loads <= max_loads);
  DCHECK(num_cores > 0);

  size_t loads = 0;

  // Setting |cores_per_load| == 0 means that no per-core limit is applied.
  if (cores_per_load == 0) {
    loads = std::numeric_limits<size_t>::max();
  } else {
    loads = num_cores / cores_per_load;
  }

  // If |max_loads| isn't zero then apply the maximum that it implies.
  if (max_loads != 0)
    loads = std::min(loads, max_loads);

  loads = std::max(loads, min_loads);

  return loads;
}

size_t SessionRestorePolicy::CalculateSimultaneousTabLoadsFromParams() const {
  // If the policy is disabled then there are no limits on the simultaneous tab
  // loads.
  if (!policy_enabled_)
    return std::numeric_limits<size_t>::max();
  return CalculateSimultaneousTabLoads(
      params_->min_simultaneous_tab_loads, params_->max_simultaneous_tab_loads,
      params_->cores_per_simultaneous_tab_load, delegate_->GetNumberOfCores());
}

void SessionRestorePolicy::SetTabLoadsStartedForTesting(
    size_t tab_loads_started) {
  tab_loads_started_ = tab_loads_started;
}

SessionRestorePolicy::Delegate::Delegate() {}

SessionRestorePolicy::Delegate::~Delegate() {}

}  // namespace resource_coordinator
