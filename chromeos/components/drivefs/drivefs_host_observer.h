// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_

namespace drivefs {
namespace mojom {
class SyncingStatus;
}  // namespace mojom

class DriveFsHostObserver {
 public:
  virtual void OnUnmounted() {}
  virtual void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) {}

 protected:
  ~DriveFsHostObserver() = default;
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_
