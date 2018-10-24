// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

namespace blink {

CanvasResourceProvider* CanvasResourceHost::ResourceProvider() const {
  return resource_provider_.get();
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceHost::ReplaceResourceProvider(
    std::unique_ptr<CanvasResourceProvider> new_resource_provider) {
  std::unique_ptr<CanvasResourceProvider> old_resource_provider =
      std::move(resource_provider_);
  resource_provider_ = std::move(new_resource_provider);
  UpdateMemoryUsage();
  return old_resource_provider;
}

void CanvasResourceHost::DiscardResourceProvider() {
  resource_provider_ = nullptr;
  UpdateMemoryUsage();
}

}  // namespace blink
