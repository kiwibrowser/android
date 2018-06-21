// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_

#include <map>

#include "base/macros.h"
#include "chromeos/components/drivefs/drivefs_host_observer.h"

namespace extensions {
namespace api {
namespace file_manager_private {
struct FileTransferStatus;
}  // namespace file_manager_private
}  // namespace api
}  // namespace extensions

namespace file_manager {

// Files app's event router handling DriveFS-related events.
class DriveFsEventRouter : public drivefs::DriveFsHostObserver {
 public:
  DriveFsEventRouter();
  virtual ~DriveFsEventRouter();

 private:
  // DriveFsHostObserver:
  void OnUnmounted() override;
  void OnSyncingStatusUpdate(
      const drivefs::mojom::SyncingStatus& status) override;

  virtual void DispatchOnFileTransfersUpdatedEvent(
      const extensions::api::file_manager_private::FileTransferStatus&
          status) = 0;

  std::map<int64_t, int64_t> group_id_to_bytes_to_transfer_;
  int64_t completed_bytes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DriveFsEventRouter);
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_DRIVEFS_EVENT_ROUTER_H_
