// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"

namespace blink {

FrameOrWorkerScheduler::LifecycleObserverHandle::LifecycleObserverHandle(
    FrameOrWorkerScheduler* scheduler,
    Observer* observer)
    : scheduler_(scheduler->GetWeakPtr()), observer_(observer) {}

FrameOrWorkerScheduler::LifecycleObserverHandle::~LifecycleObserverHandle() {
  if (scheduler_)
    scheduler_->RemoveLifecycleObserver(observer_);
}

FrameOrWorkerScheduler::FrameOrWorkerScheduler() : weak_factory_(this) {}

FrameOrWorkerScheduler::~FrameOrWorkerScheduler() {
  weak_factory_.InvalidateWeakPtrs();
}

std::unique_ptr<FrameOrWorkerScheduler::LifecycleObserverHandle>
FrameOrWorkerScheduler::AddLifecycleObserver(ObserverType type,
                                             Observer* observer) {
  DCHECK(observer);
  observer->OnLifecycleStateChanged(CalculateLifecycleState(type));
  lifecycle_observers_[observer] = type;
  return std::make_unique<LifecycleObserverHandle>(this, observer);
}

void FrameOrWorkerScheduler::RemoveLifecycleObserver(Observer* observer) {
  DCHECK(observer);
  const auto found = lifecycle_observers_.find(observer);
  DCHECK(lifecycle_observers_.end() != found);
  lifecycle_observers_.erase(found);
}

void FrameOrWorkerScheduler::NotifyLifecycleObservers() {
  for (const auto& observer : lifecycle_observers_) {
    observer.first->OnLifecycleStateChanged(
        CalculateLifecycleState(observer.second));
  }
}

base::WeakPtr<FrameOrWorkerScheduler> FrameOrWorkerScheduler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace blink
