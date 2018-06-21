// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_SESSION_RESTORE_POLICY_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_SESSION_RESTORE_POLICY_H_

#include "chrome/browser/resource_coordinator/tab_manager_features.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {

// An object that encapsulates session restore policy. For now this is surfaced
// to the TabLoader via TabLoaderDelegate, but eventually TabLoader will be
// merged into TabManager directly.
class SessionRestorePolicy {
 public:
  // Used as a testing seam.
  class Delegate;

  SessionRestorePolicy();

  // Overridden for testing.
  virtual ~SessionRestorePolicy();

  size_t simultaneous_tab_loads() const { return simultaneous_tab_loads_; }

  // Returns true if the given contents should ever be loaded by
  // session restore. If this returns false then session restore should mark the
  // tab load as deferred and move onto the next tab to restore. Note that this
  // always returns true if the policy logic is disabled.
  bool ShouldLoad(content::WebContents* contents) const;

  // Intended to be called by the policy client whenever a tab load has been
  // initiated.
  void NotifyTabLoadStarted();

  // Returns the status of the policy logic.
  bool policy_enabled() const { return policy_enabled_; }

 protected:
  // Protected so can be exposed for unittesting.

  // Full constructor for testing.
  SessionRestorePolicy(bool policy_enabled,
                       const Delegate* delegate,
                       const InfiniteSessionRestoreParams* params);

  // Helper function for computing the number of loading slots to use. All
  // parameters are exposed for testing.
  static size_t CalculateSimultaneousTabLoads(size_t min_loads,
                                              size_t max_loads,
                                              size_t cores_per_load,
                                              size_t num_cores);

  void SetTabLoadsStartedForTesting(size_t tab_loads_started);

 private:
  // This is safe to call from the constructor if |delegate_| and |params_| are
  // already initialized.
  size_t CalculateSimultaneousTabLoadsFromParams() const;

  // Initialized from the InfiniteSessionRestore policy.
  const bool policy_enabled_;

  // Delegate for interface with the system. This allows easy testing of only
  // the logic in this class.
  const Delegate* const delegate_;

  // A container for storing parsed parameters. Unless parameters are injected
  // externally this will be populated with parsed parameters.
  const InfiniteSessionRestoreParams parsed_params_;

  // The parameters being used by this policy engine. By default this is simply
  // a pointer to |parsed_params_|, but it can also point to externally
  // injected parameters for testing.
  const InfiniteSessionRestoreParams* const params_;

  // The number of simultaneous tab loads that are permitted by policy. This
  // is computed via InfiniteSessionRestore feature variations.
  const size_t simultaneous_tab_loads_;

  // The number of tab loads that have started. Every call to ShouldLoad
  // returning to true is assumed to correspond to a tab that starts loading,
  // and increments this value.
  size_t tab_loads_started_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SessionRestorePolicy);
};

// Abstracts away testing seams for the policy engine. In production code the
// default implementation wraps to base::SysInfo.
class SessionRestorePolicy::Delegate {
 public:
  Delegate();
  virtual ~Delegate();

  virtual size_t GetNumberOfCores() const = 0;
  virtual size_t GetFreeMemoryMiB() const = 0;
  virtual base::TimeTicks NowTicks() const = 0;
  virtual size_t GetSiteEngagementScore(
      content::WebContents* contents) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_SESSION_RESTORE_POLICY_H_
