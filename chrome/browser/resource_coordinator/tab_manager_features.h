// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/sys_info.h"
#include "base/time/time.h"

namespace features {

extern const base::Feature kCustomizedTabLoadTimeout;
extern const base::Feature kInfiniteSessionRestore;
extern const base::Feature kProactiveTabFreezeAndDiscard;
extern const base::Feature kSiteCharacteristicsDatabase;
extern const base::Feature kStaggeredBackgroundTabOpening;
extern const base::Feature kStaggeredBackgroundTabOpeningExperiment;
extern const base::Feature kTabRanker;

}  // namespace features

namespace resource_coordinator {

// Variations parameter names related to proactive discarding.
// See ProactiveTabFreezeAndDiscardsParams for details.
extern const char kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscard[];
extern const char kProactiveTabFreezeAndDiscard_LowLoadedTabCountParam[];
extern const char
    kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamParam[];
extern const char kProactiveTabFreezeAndDiscard_HighLoadedTabCountParam[];
extern const char kProactiveTabFreezeAndDiscard_LowOccludedTimeoutParam[];
extern const char kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutParam[];
extern const char kProactiveTabFreezeAndDiscard_HighOccludedTimeoutParam[];
extern const char kProactiveTabFreezeAndDiscard_FreezeTimeout[];

// Variations parameter names related to the site characteristics database.
// See ProactiveTabFreezeAndDiscardsParams for details.
extern const char kSiteCharacteristicsDb_FaviconUpdateObservationWindow[];
extern const char kSiteCharacteristicsDb_TitleUpdateObservationWindow[];
extern const char kSiteCharacteristicsDb_AudioUsageObservationWindow[];
extern const char kSiteCharacteristicsDb_NotificationsUsageObservationWindow[];

// Variation parameter names related to infinite session restore.
extern const char kInfiniteSessionRestore_MinSimultaneousTabLoads[];
extern const char kInfiniteSessionRestore_MaxSimultaneousTabLoads[];
extern const char kInfiniteSessionRestore_CoresPerSimultaneousTabLoad[];
extern const char kInfiniteSessionRestore_MinTabsToRestore[];
extern const char kInfiniteSessionRestore_MaxTabsToRestore[];
extern const char kInfiniteSessionRestore_MbFreeMemoryPerTabToRestore[];
// This is expressed in seconds.
extern const char kInfiniteSessionRestore_MaxTimeSinceLastUseToRestore[];
extern const char kInfiniteSessionRestore_MinSiteEngagementToRestore[];

// Default values of parameters related to the site characteristics database.
// See ProactiveTabFreezeAndDiscardsParams for details.
extern const bool kProactiveTabFreezeAndDiscard_ShouldProactivelyDiscardDefault;
extern const uint32_t kProactiveTabFreezeAndDiscard_LowLoadedTabCountDefault;
extern const uint32_t
    kProactiveTabFreezeAndDiscard_ModerateLoadedTabsPerGbRamDefault;
extern const uint32_t kProactiveTabFreezeAndDiscard_HighLoadedTabCountDefault;
extern const base::TimeDelta
    kProactiveTabFreezeAndDiscard_LowOccludedTimeoutDefault;
extern const base::TimeDelta
    kProactiveTabFreezeAndDiscard_ModerateOccludedTimeoutDefault;
extern const base::TimeDelta
    kProactiveTabFreezeAndDiscard_HighOccludedTimeoutDefault;
extern const base::TimeDelta
    kProactiveTabFreezeAndDiscard_FreezeTimeout_Default;

// Default values of parameters related to the site characteristics database.
// See SiteCharacteristicsDatabaseParams for details.
extern const base::TimeDelta
    kSiteCharacteristicsDb_FaviconUpdateObservationWindow_Default;
extern const base::TimeDelta
    kSiteCharacteristicsDb_TitleUpdateObservationWindow_Default;
extern const base::TimeDelta
    kSiteCharacteristicsDb_AudioUsageObservationWindow_Default;
extern const base::TimeDelta
    kSiteCharacteristicsDb_NotificationsUsageObservationWindow_Default;

// Default values for infinite session restore feature.
extern const uint32_t kInfiniteSessionRestore_MinSimultaneousTabLoadsDefault;
extern const uint32_t kInfiniteSessionRestore_MaxSimultaneousTabLoadsDefault;
extern const uint32_t
    kInfiniteSessionRestore_CoresPerSimultaneousTabLoadDefault;
extern const uint32_t kInfiniteSessionRestore_MinTabsToRestoreDefault;
extern const uint32_t kInfiniteSessionRestore_MaxTabsToRestoreDefault;
extern const uint32_t
    kInfiniteSessionRestore_MbFreeMemoryPerTabToRestoreDefault;
extern const base::TimeDelta
    kInfiniteSessionRestore_MaxTimeSinceLastUseToRestoreDefault;
extern const uint32_t kInfiniteSessionRestore_MinSiteEngagementToRestoreDefault;

// Parameters used by the proactive tab discarding feature.
//
// Proactive discarding has 5 key parameters:
//
// - min/max occluded timeouts
// - min/soft_max/hard_max loaded tab counts
//
// Proactive tab discarding decisions are made at two points in time:
//
// - when a new tab is created
// - when a max occluded timeout fires
//
// The following is a description of the initial simple proactive discarding
// logic. First, the number of loaded tabs is converted into one of 4 tab count
// states (LOW, MODERATE, HIGH, EXCESSIVE) using 3 simple thresholds.
//
// +-------+----------+---------+-----------+
// +  LOW  | MODERATE |  HIGH   | EXCESSIVE |
// +-------+----------+---------+-----------+
// 0      n_low      n_mod     n_high      +inf
//
// Depending on the tab count state, tabs are eligible for proactive discarding
// at different time tresholds, where the timeout is longer for lower tab
// count states. When in the low state the timeout is effectively infinite (no
// proactive discarding will occur), and when in the excessive state the timeout
// is zero (discarding will occur immediately).
//
// This logic is independent of urgent discarding, which may embark when things
// are sufficiently bad. Similarly, manual or extension driven discards can
// override this logic. Finally, proactive discarding can only discard occluded
// tabs, so it is always possible to have arbitrarily many visible tabs.
//
// NOTE: This is extremely simplistic, and by design. We will be using this to
// do a very simple "lightspeed" experiment to determine how much possible
// savings proactive discarding can hope to achieve.
struct ProactiveTabFreezeAndDiscardParams {
  ProactiveTabFreezeAndDiscardParams();
  ProactiveTabFreezeAndDiscardParams(
      const ProactiveTabFreezeAndDiscardParams& rhs);

  // Whether tabs should be proactively discarded. When the
  // |kProactiveTabFreezeAndDiscard| feature is enabled and this is false, only
  // proactive tab freezing happens.
  bool should_proactively_discard;
  // Tab count (inclusive) beyond which the state transitions to MODERATE.
  // Intended to cover the majority of simple workflows and be small enough that
  // it is very unlikely that memory pressure will be encountered with this many
  // tabs loaded.
  int low_loaded_tab_count;
  // Tab count (inclusive) beyond which the state transitions to HIGH. This
  // value is determined based on the available system memory, and is ensured to
  // be in the interval [low_loaded_tab_count, high_loaded_tab_count].
  int moderate_loaded_tab_count;
  // Tab count (inclusive) beyond which the state transitions to EXCESSIVE.
  // Not relative to system memory, as its intended to be a hard cap
  // more akin to a maximum mental model size.
  int high_loaded_tab_count;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is LOW.
  base::TimeDelta low_occluded_timeout;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is MODERATE.
  base::TimeDelta moderate_occluded_timeout;
  // Amount of time a tab must be occluded before eligible for proactive
  // discard when the tab count state is HIGH.
  base::TimeDelta high_occluded_timeout;
  // Amount of time a tab must be occluded before it is frozen.
  base::TimeDelta freeze_timeout;
};

// Parameters used by the site characteristics database.
//
// The site characteristics database tracks tab usage of a some features, a tab,
// a feature is considered as unused if it hasn't been used for a sufficiently
// long period of time while the tab was backgrounded. There's currently 4
// features we're interested in:
//
// - Favicon update
// - Title update
// - Audio usage
// - Notifications usage
struct SiteCharacteristicsDatabaseParams {
  SiteCharacteristicsDatabaseParams();
  SiteCharacteristicsDatabaseParams(
      const SiteCharacteristicsDatabaseParams& rhs);

  // Minimum observation window before considering that this website doesn't
  // update its favicon while in background.
  base::TimeDelta favicon_update_observation_window;
  // Minimum observation window before considering that this website doesn't
  // update its title while in background.
  base::TimeDelta title_update_observation_window;
  // Minimum observation window before considering that this website doesn't
  // use audio while in background.
  base::TimeDelta audio_usage_observation_window;
  // Minimum observation window before considering that this website doesn't
  // use notifications while in background.
  base::TimeDelta notifications_usage_observation_window;
};

// Parameters used by the infinite session restore feature.
struct InfiniteSessionRestoreParams {
  InfiniteSessionRestoreParams();
  InfiniteSessionRestoreParams(const InfiniteSessionRestoreParams& rhs);

  // Parameters directly retrieved from the experiment configuration.

  // The minimum number of tabs to ever load simultaneously. This can be
  // exceeded by user actions or load timeouts. See TabLoader for details.
  uint32_t min_simultaneous_tab_loads;
  // The maximum number of simultaneous tab loads that should be permitted.
  // Setting to zero means no maximum is applied.
  uint32_t max_simultaneous_tab_loads;
  // The number of CPU cores required before per permitted simultaneous tab
  // load. Setting to zero means no CPU core limit applies.
  uint32_t cores_per_simultaneous_tab_load;
  // The minimum total number of tabs to restore (if there are even that many).
  uint32_t min_tabs_to_restore;
  // The maximum total number of tabs to restore in a session restore. Setting
  // to zero means no maximum is applied.
  uint32_t max_tabs_to_restore;
  // The required amount of system free memory per tab to restore. Setting to
  // zero means no memory limit will be applied.
  uint32_t mb_free_memory_per_tab_to_restore;
  // The maximum time since last use of a tab in order for it to be restored.
  // Setting to zero means this logic does not apply.
  base::TimeDelta max_time_since_last_use_to_restore;
  // The minimum site engagement score in order for a tab to be restored.
  // Setting this to zero means all tabs will be restored regardless of the
  // site engagement score.
  uint32_t min_site_engagement_to_restore;
};

// Gets parameters for the proactive tab discarding feature. This does no
// parameter validation, and sets the default values if the feature is not
// enabled.
ProactiveTabFreezeAndDiscardParams GetProactiveTabFreezeAndDiscardParams(
    int memory_in_gb = base::SysInfo::AmountOfPhysicalMemory() /
                       (1024 * 1024 * 1024));

// Return a static ProactiveTabFreezeAndDiscardParams object that can be used by
// all the classes that need one.
const ProactiveTabFreezeAndDiscardParams&
GetStaticProactiveTabFreezeAndDiscardParams();

base::TimeDelta GetTabLoadTimeout(const base::TimeDelta& default_timeout);

// Gets parameters for the site characteristics database feature. This does no
// parameter validation, and sets the default values if the feature is not
// enabled.
SiteCharacteristicsDatabaseParams GetSiteCharacteristicsDatabaseParams();

// Return a static SiteCharacteristicsDatabaseParams object that can be used by
// all the classes that need one.
const SiteCharacteristicsDatabaseParams&
GetStaticSiteCharacteristicsDatabaseParams();

// Gets parameters for the infinite session restore feature.
InfiniteSessionRestoreParams GetInfiniteSessionRestoreParams();

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_FEATURES_H_
