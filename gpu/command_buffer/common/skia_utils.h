// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_

#include <memory>
#include "gpu/raster_export.h"

namespace gpu {
namespace raster {

RASTER_EXPORT void DetermineGrCacheLimitsFromAvailableMemory(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes);

RASTER_EXPORT void DefaultGrCacheLimitsForTests(
    size_t* max_resource_cache_bytes,
    size_t* max_glyph_cache_texture_bytes);

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SKIA_UTILS_H_
