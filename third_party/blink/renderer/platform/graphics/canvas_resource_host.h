// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class CanvasResourceProvider;

class PLATFORM_EXPORT CanvasResourceHost {
 public:
  virtual ~CanvasResourceHost() = default;
  virtual void NotifyGpuContextLost() = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void RestoreCanvasMatrixClipStack(PaintCanvas*) const = 0;
  virtual void UpdateMemoryUsage() = 0;

  CanvasResourceProvider* ResourceProvider() const;

  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>);

  virtual void DiscardResourceProvider();

 private:
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
};

}  // namespace blink

#endif
