// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_host.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

CanvasRenderingContextHost::CanvasRenderingContextHost() = default;

scoped_refptr<StaticBitmapImage>
CanvasRenderingContextHost::CreateTransparentImage(const IntSize& size) const {
  if (!IsValidImageSize(size))
    return nullptr;
  CanvasColorParams color_params = CanvasColorParams();
  if (RenderingContext())
    color_params = RenderingContext()->ColorParams();
  SkImageInfo info = SkImageInfo::Make(
      size.Width(), size.Height(), color_params.GetSkColorType(),
      kPremul_SkAlphaType, color_params.GetSkColorSpaceForSkSurfaces());
  sk_sp<SkSurface> surface =
      SkSurface::MakeRaster(info, info.minRowBytes(), nullptr);
  if (!surface)
    return nullptr;
  return StaticBitmapImage::Create(surface->makeImageSnapshot());
}

bool CanvasRenderingContextHost::IsPaintable() const {
  return (RenderingContext() && RenderingContext()->IsPaintable()) ||
         IsValidImageSize(Size());
}

void CanvasRenderingContextHost::RestoreCanvasMatrixClipStack(
    PaintCanvas* canvas) const {
  if (RenderingContext())
    RenderingContext()->RestoreCanvasMatrixClipStack(canvas);
}

bool CanvasRenderingContextHost::Is3d() const {
  return RenderingContext() && RenderingContext()->Is3d();
}

bool CanvasRenderingContextHost::Is2d() const {
  return RenderingContext() && RenderingContext()->Is2d();
}

CanvasResourceProvider*
CanvasRenderingContextHost::GetOrCreateCanvasResourceProvider() {
  if (!ResourceProvider() && !did_fail_to_create_resource_provider_) {
    resource_provider_is_clear_ = true;
    if (IsValidImageSize(Size())) {
      if (Is3d()) {
        ReplaceResourceProvider(CanvasResourceProvider::Create(
            Size(),
            Platform::Current()->IsGpuCompositingDisabled()
                ? CanvasResourceProvider::kSoftwareResourceUsage
                : CanvasResourceProvider::kAcceleratedResourceUsage,
            SharedGpuContext::ContextProviderWrapper(),
            0,  // msaa_sample_count
            ColorParams(), CanvasResourceProvider::kDefaultPresentationMode,
            nullptr));  // canvas_resource_dispatcher
      } else {
        // 2d context
        // TODO: move resource provider ownership from canvas 2d layer bridge
        // to here.
        NOTREACHED();
      }
    }
    if (!ResourceProvider()) {
      did_fail_to_create_resource_provider_ = true;
    }
  }
  return ResourceProvider();
}

CanvasColorParams CanvasRenderingContextHost::ColorParams() const {
  if (RenderingContext())
    return RenderingContext()->ColorParams();
  return CanvasColorParams();
}

ScriptPromise CanvasRenderingContextHost::convertToBlob(
    ScriptState* script_state,
    const ImageEncodeOptions& options,
    ExceptionState& exception_state) const {
  WTF::String object_name = "Canvas";
  if (this->IsOffscreenCanvas())
    object_name = "OffscreenCanvas";
  std::stringstream error_msg;

  if (this->IsOffscreenCanvas() && this->IsNeutered()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "OffscreenCanvas object is detached.");
    return ScriptPromise();
  }

  if (!this->OriginClean()) {
    error_msg << "Tainted " << object_name << " may not be exported.";
    exception_state.ThrowSecurityError(error_msg.str().c_str());
    return ScriptPromise();
  }

  if (!this->IsPaintable() || Size().IsEmpty()) {
    error_msg << "The size of " << object_name << " iz zero.";
    exception_state.ThrowDOMException(DOMExceptionCode::kIndexSizeError,
                                      error_msg.str().c_str());
    return ScriptPromise();
  }

  if (!RenderingContext()) {
    error_msg << object_name << " has no rendering context.";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      error_msg.str().c_str());
    return ScriptPromise();
  }

  double start_time = WTF::CurrentTimeTicksInSeconds();
  scoped_refptr<StaticBitmapImage> image_bitmap =
      RenderingContext()->GetImage(kPreferNoAcceleration);
  if (image_bitmap) {
    ScriptPromiseResolver* resolver =
        ScriptPromiseResolver::Create(script_state);
    CanvasAsyncBlobCreator::ToBlobFunctionType function_type =
        CanvasAsyncBlobCreator::kHTMLCanvasConvertToBlobPromise;
    if (this->IsOffscreenCanvas()) {
      function_type =
          CanvasAsyncBlobCreator::kOffscreenCanvasConvertToBlobPromise;
    }
    CanvasAsyncBlobCreator* async_creator = CanvasAsyncBlobCreator::Create(
        image_bitmap, options, function_type, start_time,
        ExecutionContext::From(script_state), resolver);
    async_creator->ScheduleAsyncBlobCreation(options.quality());
    return resolver->Promise();
  }
  exception_state.ThrowDOMException(DOMExceptionCode::kNotReadableError,
                                    "Readback of the source image has failed.");
  return ScriptPromise();
}

}  // namespace blink
