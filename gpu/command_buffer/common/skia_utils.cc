// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/skia_utils.h"

#include "base/sys_info.h"
#include "build/build_config.h"

namespace gpu {
namespace raster {

void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes) {
  // Default limits.
  constexpr size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;
  constexpr size_t kMaxDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;

  *max_resource_cache_bytes = kMaxGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kMaxDefaultGlyphCacheTextureBytes;

// We can't call AmountOfPhysicalMemory under NACL, so leave the default.
#if !defined(OS_NACL)
  // The limit of the bytes allocated toward GPU resources in the GrContext's
  // GPU cache.
  constexpr size_t kMaxLowEndGaneshResourceCacheBytes = 48 * 1024 * 1024;
  constexpr size_t kMaxHighEndGaneshResourceCacheBytes = 256 * 1024 * 1024;
  // Limits for glyph cache textures.
  constexpr size_t kMaxLowEndGlyphCacheTextureBytes = 1024 * 512 * 4;
  // High-end / low-end memory cutoffs.
  constexpr int64_t kHighEndMemoryThreshold = (int64_t)4096 * 1024 * 1024;
  constexpr int64_t kLowEndMemoryThreshold = (int64_t)512 * 1024 * 1024;

  int64_t amount_of_physical_memory = base::SysInfo::AmountOfPhysicalMemory();
  if (amount_of_physical_memory <= kLowEndMemoryThreshold) {
    *max_resource_cache_bytes = kMaxLowEndGaneshResourceCacheBytes;
    *max_glyph_cache_texture_bytes = kMaxLowEndGlyphCacheTextureBytes;
  } else if (amount_of_physical_memory >= kHighEndMemoryThreshold) {
    *max_resource_cache_bytes = kMaxHighEndGaneshResourceCacheBytes;
  }
#endif
}

void DefaultGrCacheLimitsForTests(size_t* max_resource_cache_bytes,
                                  size_t* max_glyph_cache_texture_bytes) {
  constexpr size_t kDefaultGlyphCacheTextureBytes = 2048 * 1024 * 4;
  constexpr size_t kDefaultGaneshResourceCacheBytes = 96 * 1024 * 1024;
  *max_resource_cache_bytes = kDefaultGaneshResourceCacheBytes;
  *max_glyph_cache_texture_bytes = kDefaultGlyphCacheTextureBytes;
}

}  // namespace raster
}  // namespace gpu
