// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_switches.h"

namespace switches {

// Disable workarounds for various GPU driver bugs.
const char kDisableGpuDriverBugWorkarounds[] =
    "disable-gpu-driver-bug-workarounds";

// Disable GPU rasterization, i.e. rasterize on the CPU only.
// Overrides the kEnableGpuRasterization and kForceGpuRasterization flags.
const char kDisableGpuRasterization[] = "disable-gpu-rasterization";

// Allow heuristics to determine when a layer tile should be drawn with the
// Skia GPU backend. Only valid with GPU accelerated compositing +
// impl-side painting.
const char kEnableGpuRasterization[] = "enable-gpu-rasterization";

// Passes encoded GpuPreferences to GPU process.
const char kGpuPreferences[] = "gpu-preferences";

// Ignores GPU blacklist.
const char kIgnoreGpuBlacklist[] = "ignore-gpu-blacklist";

// Select a different set of GPU blacklist entries with the specificed
// test_group ID.
const char kGpuBlacklistTestGroup[] = "gpu-blacklist-test-group";

// Select a different set of GPU driver bug list entries with the specificed
// test_group ID.
const char kGpuDriverBugListTestGroup[] = "gpu-driver-bug-list-test-group";

// Allows explicitly specifying the shader disk cache size for embedded devices.
// Default value is 6MB. On Android, 2MB is default and 128KB for low-end
// devices.
const char kShaderDiskCacheSizeKB[] = "shader-disk-cache-size-kb";

// Use GpuFence objects to synchronize display of overlay planes.
const char kUseGpuFencesForOverlayPlanes[] =
    "use-gpu-fences-for-overlay-planes";

// Set the antialiasing method used for webgl. (none, explicit, implicit, or
// screenspace)
const char kWebglAntialiasingMode[] = "webgl-antialiasing-mode";

// Set a default sample count for webgl if msaa is enabled.
const char kWebglMSAASampleCount[] = "webgl-msaa-sample-count";

}  // namespace switches
