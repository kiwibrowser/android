// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class FakeCanvasResourceHost : public CanvasResourceHost {
 public:
  ~FakeCanvasResourceHost() override {}
  void NotifyGpuContextLost() override {}
  void SetNeedsCompositingUpdate() override {}
  void RestoreCanvasMatrixClipStack(PaintCanvas*) const override {}
  void UpdateMemoryUsage() override {}
};

}  // namespace blink

#endif
