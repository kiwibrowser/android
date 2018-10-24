// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/lifecycle_unit_base.h"

#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {

LifecycleUnitBase::LifecycleUnitBase(content::Visibility visibility)
    : last_active_time_(visibility == content::Visibility::VISIBLE
                            ? base::TimeTicks::Max()
                            : NowTicks()) {}

LifecycleUnitBase::~LifecycleUnitBase() = default;

int32_t LifecycleUnitBase::GetID() const {
  return id_;
}

LifecycleUnitState LifecycleUnitBase::GetState() const {
  return state_;
}

base::TimeTicks LifecycleUnitBase::GetLastActiveTime() const {
  return last_active_time_;
}

void LifecycleUnitBase::AddObserver(LifecycleUnitObserver* observer) {
  observers_.AddObserver(observer);
}

void LifecycleUnitBase::RemoveObserver(LifecycleUnitObserver* observer) {
  observers_.RemoveObserver(observer);
}

ukm::SourceId LifecycleUnitBase::GetUkmSourceId() const {
  return ukm::kInvalidSourceId;
}

void LifecycleUnitBase::SetState(LifecycleUnitState state,
                                 LifecycleUnitStateChangeReason reason) {
  if (state == state_)
    return;
  LifecycleUnitState last_state = state_;
  state_ = state;
  OnLifecycleUnitStateChanged(last_state, reason);
  for (auto& observer : observers_)
    observer.OnLifecycleUnitStateChanged(this, last_state);
}

void LifecycleUnitBase::OnLifecycleUnitStateChanged(
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {}

void LifecycleUnitBase::OnLifecycleUnitVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE)
    last_active_time_ = base::TimeTicks::Max();
  else if (last_active_time_.is_max())
    last_active_time_ = NowTicks();

  for (auto& observer : observers_)
    observer.OnLifecycleUnitVisibilityChanged(this, visibility);
}

void LifecycleUnitBase::OnLifecycleUnitDestroyed() {
  for (auto& observer : observers_)
    observer.OnLifecycleUnitDestroyed(this);
}

int32_t LifecycleUnitBase::next_id_ = 0;

}  // namespace resource_coordinator
