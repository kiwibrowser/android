// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_

#include <stdint.h>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/decision_details.h"
#include "chrome/browser/resource_coordinator/discard_reason.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "content/public/browser/visibility.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitLoadingState;
using ::mojom::LifecycleUnitState;

class DecisionDetails;
class LifecycleUnitObserver;
class TabLifecycleUnitExternal;

// A LifecycleUnit represents a unit that can switch between the "loaded" and
// "discarded" states. When it is loaded, the unit uses system resources and
// provides functionality to the user. When it is discarded, the unit doesn't
// use any system resource.
class LifecycleUnit {
 public:
  // Used to sort LifecycleUnit by importance using a reactivation score or the
  // last focused time.
  // The most important LifecycleUnit has the greatest SortKey.
  struct SortKey {
    // kMaxScore is used when a SortKey should rank ahead of any other SortKey.
    // Two SortKeys with kMaxScore are compared using |last_focused_time|.
    static constexpr float kMaxScore = std::numeric_limits<float>::max();

    SortKey();

    // Creates a SortKey based on the LifecycleUnit's last focused time.
    explicit SortKey(base::TimeTicks last_focused_time);

    // Creates a SortKey based on a score calculated for the LifecycleUnit and
    // the last focused time. Used when the TabRanker feature is enabled.
    SortKey(float score, base::TimeTicks last_focused_time);

    SortKey(const SortKey& other);

    bool operator<(const SortKey& other) const;
    bool operator>(const SortKey& other) const;

    // Abstract importance score calculated by the Tab Ranker where a higher
    // score suggests the tab is more likely to be reactivated.
    // kMaxScore if the LifecycleUnit is currently focused.
    base::Optional<float> score;

    // Last time at which the LifecycleUnit was focused. base::TimeTicks::Max()
    // if the LifecycleUnit is currently focused.
    // Used when the TabRanker feature is disabled. Also used as a tiebreaker
    // when two scores are the same.
    base::TimeTicks last_focused_time;
  };

  virtual ~LifecycleUnit();

  // Returns the TabLifecycleUnitExternal associated with this LifecycleUnit, if
  // any.
  virtual TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() = 0;

  // Returns a unique id representing this LifecycleUnit.
  virtual int32_t GetID() const = 0;

  // Returns a title describing this LifecycleUnit, or an empty string if no
  // title is available.
  virtual base::string16 GetTitle() const = 0;

  // Returns the last time at which the LifecycleUnit was focused, or
  // base::TimeTicks::Max() if the LifecycleUnit is currently focused.
  virtual base::TimeTicks GetLastFocusedTime() const = 0;

  // Returns the current visibility of this LifecycleUnit.
  virtual content::Visibility GetVisibility() const = 0;

  // Returns TimeTicks::Max() if the LifecycleUnit is currently visible, the
  // last time at which the LifecycleUnit was visible if it's not currently
  // visible but has been visible in the past, the LifecycleUnit creation time
  // otherwise.
  virtual base::TimeTicks GetLastActiveTime() const = 0;

  // Returns the loading state associated with a LifecycleUnit.
  virtual LifecycleUnitLoadingState GetLoadingState() const = 0;

  // Returns the process hosting this LifecycleUnit. Used to distribute OOM
  // scores.
  //
  // TODO(fdoray): Change this to take into account the fact that a
  // LifecycleUnit can be hosted in multiple processes. https://crbug.com/775644
  virtual base::ProcessHandle GetProcessHandle() const = 0;

  // Returns a key that can be used to evaluate the relative importance of this
  // LifecycleUnit. This key may not be trivial to calculate, so this should not
  // be called repeatedly if the value will be reused, e.g. during a sort.
  //
  // TODO(fdoray): Figure out if GetSortKey() and CanDiscard() should be
  // replaced with a method that returns a numeric value representing the
  // expected user pain caused by a discard. A values above a given threshold
  // would be equivalent to CanDiscard() returning false for a given
  // DiscardReason. https://crbug.com/775644
  virtual SortKey GetSortKey() const = 0;

  // Returns the current state of this LifecycleUnit.
  virtual LifecycleUnitState GetState() const = 0;

  // Request that the LifecycleUnit be loaded, return true if the request is
  // successful.
  virtual bool Load() = 0;

  // Returns the estimated number of kilobytes that would be freed if this
  // LifecycleUnit was discarded.
  //
  // TODO(fdoray): Consider exposing this only on a new class that represents a
  // group of LifecycleUnits. It is easier to compute memory consumption
  // accurately for a group of LifecycleUnits that live in the same process(es)
  // than for individual LifecycleUnits. https://crbug.com/775644
  virtual int GetEstimatedMemoryFreedOnDiscardKB() const = 0;

  // Whether memory can be purged in the process hosting this LifecycleUnit.
  //
  // TODO(fdoray): This method should be on a class that represents a process,
  // not on a LifecycleUnit. https://crbug.com/775644
  virtual bool CanPurge() const = 0;

  // Returns true if this LifecycleUnit can be frozen. Full details regarding
  // the policy decision are recorded in |decision_details|, for logging.
  // Returning false but with an empty |decision_details| means the transition
  // is not possible for a trivial reason that doesn't need to be reported (ie,
  // the page is already frozen).
  virtual bool CanFreeze(DecisionDetails* decision_details) const = 0;

  // Returns true if this LifecycleUnit can be discarded. Full details regarding
  // the policy decision are recorded in the |decision_details|, for logging.
  // Returning false but with an empty |decision_details| means the transition
  // is not possible for a trivial reason that doesn't need to be reported
  // (ie, the page is already discarded).
  virtual bool CanDiscard(DiscardReason reason,
                          DecisionDetails* decision_details) const = 0;

  // Request that the LifecycleUnit be frozen, return true if the request is
  // successfully sent.
  virtual bool Freeze() = 0;

  // Unfreezes this LifecycleUnit. Returns true on success.
  virtual bool Unfreeze() = 0;

  // Discards this LifecycleUnit.
  //
  // TODO(fdoray): Consider handling urgent discard with groups of
  // LifecycleUnits. On urgent discard, we want to minimize memory accesses. It
  // is easier to achieve that if we discard a group of LifecycleUnits that live
  // in the same process(es) than if we discard individual LifecycleUnits.
  // https://crbug.com/775644
  virtual bool Discard(DiscardReason discard_reason) = 0;

  // Adds/removes an observer to this LifecycleUnit.
  virtual void AddObserver(LifecycleUnitObserver* observer) = 0;
  virtual void RemoveObserver(LifecycleUnitObserver* observer) = 0;

  // Returns the UKM source ID associated with this LifecycleUnit, if it has
  // one.
  virtual ukm::SourceId GetUkmSourceId() const = 0;
};

using LifecycleUnitSet = base::flat_set<LifecycleUnit*>;
using LifecycleUnitVector = std::vector<LifecycleUnit*>;

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
