// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEPENDENCIES_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEPENDENCIES_H_

#include "content/common/content_export.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"

namespace content {

// Browser-provided GPU dependencies for video capture.
class CONTENT_EXPORT VideoCaptureDependencies {
 public:
  static void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest accelerator);
  static void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest accelerator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_DEPENDENCIES_H_
