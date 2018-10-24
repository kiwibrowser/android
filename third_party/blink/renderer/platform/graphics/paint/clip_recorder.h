// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_RECORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_RECORDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class GraphicsContext;

class PLATFORM_EXPORT ClipRecorder {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  ClipRecorder(GraphicsContext&,
               const DisplayItemClient&,
               DisplayItem::Type,
               const IntRect& clip_rect);
  ~ClipRecorder();

 private:
  const DisplayItemClient& client_;
  GraphicsContext& context_;
  DisplayItem::Type type_;

  DISALLOW_COPY_AND_ASSIGN(ClipRecorder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CLIP_RECORDER_H_
