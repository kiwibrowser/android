// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_UNALIGNED_SHARED_MEMORY_H_
#define MEDIA_BASE_UNALIGNED_SHARED_MEMORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "media/base/media_export.h"

namespace media {

// Wrapper over base::SharedMemory that can be mapped at unaligned offsets.
class MEDIA_EXPORT UnalignedSharedMemory {
 public:
  // Creates an |UnalignedSharedMemory| instance from a
  // |SharedMemoryHandle|. |size| sets the maximum size that may be mapped.
  UnalignedSharedMemory(const base::SharedMemoryHandle& handle,
                        size_t size,
                        bool read_only);
  ~UnalignedSharedMemory();

  // Map the shared memory region. Note that the passed |size| parameter should
  // be less than or equal to |size()|.
  bool MapAt(off_t offset, size_t size);
  size_t size() const { return size_; }
  void* memory() const;

 private:
  base::SharedMemory shm_;

  // The size of the region associated with |shm_|.
  size_t size_;

  // Offset withing |shm_| memory that data has been mapped; strictly less than
  // base::SysInfo::VMAllocationGranularity().
  size_t misalignment_;

  DISALLOW_COPY_AND_ASSIGN(UnalignedSharedMemory);
};

}  // namespace media

#endif  // MEDIA_BASE_UNALIGNED_SHARED_MEMORY_H_
