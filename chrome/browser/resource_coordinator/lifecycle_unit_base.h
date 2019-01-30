// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "content/public/browser/visibility.h"

namespace resource_coordinator {

using ::mojom::LifecycleUnitState;
using ::mojom::LifecycleUnitStateChangeReason;

// Base class for a LifecycleUnit.
class LifecycleUnitBase : public LifecycleUnit {
 public:
  explicit LifecycleUnitBase(content::Visibility visibility);
  ~LifecycleUnitBase() override;

  // LifecycleUnit:
  int32_t GetID() const override;
  base::TimeTicks GetLastActiveTime() const override;
  LifecycleUnitState GetState() const override;
  void AddObserver(LifecycleUnitObserver* observer) override;
  void RemoveObserver(LifecycleUnitObserver* observer) override;
  ukm::SourceId GetUkmSourceId() const override;

 protected:
  // Sets the state of this LifecycleUnit to |state| and notifies observers.
  // |reason| indicates what caused the state change.
  void SetState(LifecycleUnitState state,
                LifecycleUnitStateChangeReason reason);

  // Invoked when the state of the LifecycleUnit changes, before external
  // observers are notified. Derived classes can override to add their own
  // logic. The default implementation is empty. |last_state| is the state
  // before the change and |reason| indicates what caused the change.
  virtual void OnLifecycleUnitStateChanged(
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason);

  // Notifies observers that the visibility of the LifecycleUnit has changed.
  void OnLifecycleUnitVisibilityChanged(content::Visibility visibility);

  // Notifies observers that the LifecycleUnit is being destroyed. This is
  // invoked by derived classes rather than by the base class to avoid notifying
  // observers when the LifecycleUnit has been partially destroyed.
  void OnLifecycleUnitDestroyed();

 private:
  static int32_t next_id_;

  // A unique id representing this LifecycleUnit.
  const int32_t id_ = ++next_id_;

  // Current state of this LifecycleUnit.
  LifecycleUnitState state_ = LifecycleUnitState::ACTIVE;

  // TODO(fdoray): Use WebContents::GetLastActiveTime() instead of tracking a
  // separate last active time here. For this to work,
  // WebContents::GetLastActiveTime() will have to be updated to return the last
  // time at which the WebContents was active, rather than the last time at
  // which it was activated.
  base::TimeTicks last_active_time_;

  base::ObserverList<LifecycleUnitObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleUnitBase);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_
