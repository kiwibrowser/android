// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

std::unique_ptr<cc::ServiceTransferCacheEntry> CreateEntry(size_t size) {
  auto entry = std::make_unique<cc::ServiceRawMemoryTransferCacheEntry>();
  std::vector<uint8_t> data(size, 0u);
  entry->Deserialize(nullptr, data);
  return entry;
}

TEST(ServiceTransferCacheTest, EnforcesOnPurgeMemory) {
  ServiceTransferCache cache;
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;

  cache.CreateLocalEntry(++entry_id, CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
  cache.OnPurgeMemory();
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);
}

TEST(ServiceTransferCacheTest, EnforcesMemoryStateSUSPENDED) {
  ServiceTransferCache cache;
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;

  // Nothing should be cached in suspended state.
  cache.OnMemoryStateChange(base::MemoryState::SUSPENDED);
  cache.CreateLocalEntry(++entry_id, CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);
}

TEST(ServiceTransferCacheTest, EnforcesMemoryStateNORMAL) {
  ServiceTransferCache cache;
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;

  // Normal state, keep what's in the cache.
  cache.CreateLocalEntry(++entry_id, CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
  cache.OnMemoryStateChange(base::MemoryState::NORMAL);
  cache.CreateLocalEntry(++entry_id, CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 2);
}

}  // namespace
}  // namespace gpu
