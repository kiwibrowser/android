// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_TEST_TEST_SHARED_BITMAP_MANAGER_H_

#include <map>
#include <set>

#include "base/sequence_checker.h"
#include "components/viz/service/display/shared_bitmap_manager.h"

namespace base {
class SharedMemory;
}  // namespace base

namespace viz {

class TestSharedBitmapManager : public SharedBitmapManager {
 public:
  TestSharedBitmapManager();
  ~TestSharedBitmapManager() override;

  // SharedBitmapManager implementation.
  std::unique_ptr<SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      ResourceFormat format,
      const SharedBitmapId& id) override;
  base::UnguessableToken GetSharedBitmapTracingGUIDFromId(
      const SharedBitmapId& id) override;
  bool ChildAllocatedSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                                  const SharedBitmapId& id) override;
  void ChildDeletedSharedBitmap(const SharedBitmapId& id) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::map<SharedBitmapId, base::SharedMemory*> bitmap_map_;
  std::map<SharedBitmapId, std::unique_ptr<base::SharedMemory>> owned_map_;
  std::set<SharedBitmapId> notified_set_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_SHARED_BITMAP_MANAGER_H_
