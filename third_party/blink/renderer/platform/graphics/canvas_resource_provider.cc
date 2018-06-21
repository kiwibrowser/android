// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

// CanvasResourceProviderTexture
//==============================================================================
//
// * Renders to a texture managed by skia. Mailboxes are straight GL textures.
// * Layers are not overlay candidates

class CanvasResourceProviderTexture : public CanvasResourceProvider {
 public:
  CanvasResourceProviderTexture(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)),
        msaa_sample_count_(msaa_sample_count) {}

  ~CanvasResourceProviderTexture() override = default;

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }

  GLuint GetBackingTextureHandleForOverwrite() override {
    GrBackendTexture backend_texture = GetSkSurface()->getBackendTexture(
        SkSurface::kDiscardWrite_TextureHandleAccess);
    if (!backend_texture.isValid())
      return 0;
    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info))
      return 0;
    return info.fID;
  }

 protected:
  scoped_refptr<CanvasResource> ProduceFrame() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderTexture::ProduceFrame");
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    auto* gl = ContextGL();
    DCHECK(gl);

    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters.
      // If this issue with readback affecting state is ever fixed, then we'll
      // have to do this instead of triggering a copy-on-write:
      // static_cast<AcceleratedStaticBitmapImage*>(image.get())
      //  ->RetainOriginalSkImageForCopyOnWrite();
      GetSkSurface()->notifyContentWillChange(
          SkSurface::kRetain_ContentChangeMode);
    }

    sk_sp<SkImage> skia_image = GetSkSurface()->makeImageSnapshot();
    if (!skia_image)
      return nullptr;
    DCHECK(skia_image->isTextureBacked());

    scoped_refptr<StaticBitmapImage> image =
        StaticBitmapImage::Create(skia_image, ContextProviderWrapper());

    scoped_refptr<CanvasResource> resource = CanvasResourceBitmap::Create(
        image, CreateWeakPtr(), FilterQuality(), ColorParams());
    if (!resource)
      return nullptr;

    return resource;
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderTexture::CreateSkSurface");

    if (IsGpuContextLost())
      return nullptr;
    auto* gr = GetGrContext();
    DCHECK(gr);

    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, info,
                                       msaa_sample_count_,
                                       ColorParams().GetSkSurfaceProps());
  }

  const unsigned msaa_sample_count_;
};

// CanvasResourceProviderTextureGpuMemoryBuffer
//==============================================================================
//
// * Renders to a texture managed by skia. Mailboxes are
//     gpu-accelerated platform native surfaces.
// * Layers are overlay candidates

class CanvasResourceProviderTextureGpuMemoryBuffer final
    : public CanvasResourceProviderTexture {
 public:
  CanvasResourceProviderTextureGpuMemoryBuffer(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderTexture(size,
                                      msaa_sample_count,
                                      color_params,
                                      std::move(context_provider_wrapper),
                                      std::move(resource_dispatcher)) {}

  ~CanvasResourceProviderTextureGpuMemoryBuffer() override = default;

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0(
        "blink",
        "CanvasResourceProviderTextureGpuMemoreBuffer::CreateResource");
    constexpr bool is_accelerated = true;
    return CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderTextureGpuMemoreBuffer::ProduceFrame");

    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // GpuMemoryBuffer creation failed, fallback to Texture resource
      return CanvasResourceProviderTexture::ProduceFrame();
    }

    sk_sp<SkImage> image = GetSkSurface()->makeImageSnapshot();
    if (!image)
      return nullptr;
    DCHECK(image->isTextureBacked());

    GrBackendTexture backend_texture = image->getBackendTexture(true);
    DCHECK(backend_texture.isValid());

    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info))
      return nullptr;

    GLuint skia_texture_id = info.fID;
    output_resource->CopyFromTexture(skia_texture_id,
                                     ColorParams().GLInternalFormat(),
                                     ColorParams().GLType());

    return output_resource;
  }
};

// CanvasResourceProviderBitmap
//==============================================================================
//
// * Renders to a skia RAM-backed bitmap
// * Mailboxing is not supported : cannot be directly composited

class CanvasResourceProviderBitmap : public CanvasResourceProvider {
 public:
  CanvasResourceProviderBitmap(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(size,
                               color_params,
                               nullptr /*context_provider_wrapper*/,
                               std::move(resource_dispatcher)) {}

  ~CanvasResourceProviderBitmap() override = default;

  bool IsValid() const final { return GetSkSurface(); }
  bool IsAccelerated() const final { return false; }

 private:
  scoped_refptr<CanvasResource> ProduceFrame() override {
    NOTREACHED();  // Not directly compositable.
    return nullptr;
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::CreateSkSurface");

    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRaster(info, ColorParams().GetSkSurfaceProps());
  }
};

// CanvasResourceProviderRamGpuMemoryBuffer
//==============================================================================
//
// * Renders to a ram memory buffer managed by skia
// * Uses GpuMemoryBuffer to pass frames to the compositor
// * Layers are overlay candidates

class CanvasResourceProviderRamGpuMemoryBuffer final
    : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderRamGpuMemoryBuffer(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     color_params,
                                     std::move(resource_dispatcher)) {}

  ~CanvasResourceProviderRamGpuMemoryBuffer() override = default;

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderRamGpuMemoryBuffer::CreateResource");

    constexpr bool is_accelerated = false;
    return CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderRamGpuMemoryBuffer::ProduceFrame");

    DCHECK(GetSkSurface());

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // Not compositable without a GpuMemoryBuffer
      return nullptr;
    }

    sk_sp<SkImage> image = GetSkSurface()->makeImageSnapshot();
    if (!image)
      return nullptr;
    DCHECK(!image->isTextureBacked());

    output_resource->TakeSkImage(std::move(image));

    return output_resource;
  }
};

// CanvasResourceProviderSharedBitmap
//==============================================================================
//
// * Renders to a shared memory bitmap
// * Uses SharedBitmaps to pass frames directly to the compositor

class CanvasResourceProviderSharedBitmap : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderSharedBitmap(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     color_params,
                                     std::move(resource_dispatcher)) {
    DCHECK(ResourceDispatcher());
  }
  ~CanvasResourceProviderSharedBitmap() override = default;

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    return CanvasResourceSharedBitmap::Create(Size(), ColorParams(),
                                              CreateWeakPtr(), FilterQuality());
  }

  scoped_refptr<CanvasResource> ProduceFrame() final {
    DCHECK(GetSkSurface());

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // Not compositable without a SharedBitmap
      return nullptr;
    }

    sk_sp<SkImage> image = GetSkSurface()->makeImageSnapshot();
    if (!image)
      return nullptr;
    DCHECK(!image->isTextureBacked());

    output_resource->TakeSkImage(std::move(image));

    return output_resource;
  }

  base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher_;
};

// CanvasResourceProvider base class implementation
//==============================================================================

enum ResourceType {
  kTextureGpuMemoryBufferResourceType,
  kRamGpuMemoryBufferResourceType,
  kSharedBitmapResourceType,
  kTextureResourceType,
  kBitmapResourceType,
};

constexpr ResourceType kSoftwareCompositedFallbackList[] = {
    kRamGpuMemoryBufferResourceType, kSharedBitmapResourceType,
    kBitmapResourceType,
};

constexpr ResourceType kSoftwareFallbackList[] = {
    kBitmapResourceType,
};

constexpr ResourceType kAcceleratedFallbackList[] = {
    kTextureResourceType, kBitmapResourceType,
};

constexpr ResourceType kAcceleratedCompositedFallbackList[] = {
    kTextureGpuMemoryBufferResourceType,
    kTextureResourceType,
    kRamGpuMemoryBufferResourceType,
    kSharedBitmapResourceType,
    kBitmapResourceType,
};

std::unique_ptr<CanvasResourceProvider> CanvasResourceProvider::Create(
    const IntSize& size,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    const CanvasColorParams& color_params,
    PresentationMode presentation_mode,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher) {
  const ResourceType* resource_type_fallback_list = nullptr;
  size_t list_length = 0;

  switch (usage) {
    case kSoftwareResourceUsage:
      resource_type_fallback_list = kSoftwareFallbackList;
      list_length = arraysize(kSoftwareFallbackList);
      break;
    case kSoftwareCompositedResourceUsage:
      resource_type_fallback_list = kSoftwareCompositedFallbackList;
      list_length = arraysize(kSoftwareCompositedFallbackList);
      break;
    case kAcceleratedResourceUsage:
      resource_type_fallback_list = kAcceleratedFallbackList;
      list_length = arraysize(kAcceleratedFallbackList);
      break;
    case kAcceleratedCompositedResourceUsage:
      resource_type_fallback_list = kAcceleratedCompositedFallbackList;
      list_length = arraysize(kAcceleratedCompositedFallbackList);
      break;
  }

  std::unique_ptr<CanvasResourceProvider> provider;
  for (size_t i = 0; i < list_length; ++i) {
    // Note: We are deliberately not using std::move() on
    // context_provider_wrapper and resource_dispatcher to ensure that the
    // pointers remain valid for the next iteration of this loop if necessary.
    switch (resource_type_fallback_list[i]) {
      case kTextureGpuMemoryBufferResourceType:
        DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
        if (presentation_mode !=
            CanvasResourceProvider::kAllowImageChromiumPresentationMode)
          continue;
        if (!gpu::IsImageFromGpuMemoryBufferFormatSupported(
                color_params.GetBufferFormat(),
                context_provider_wrapper->ContextProvider()
                    ->GetCapabilities())) {
          continue;
        }
        if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(
                gfx::Size(size), color_params.GetBufferFormat())) {
          continue;
        }
        DCHECK(gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
            color_params.GLInternalFormat(), color_params.GetBufferFormat()));
        provider =
            std::make_unique<CanvasResourceProviderTextureGpuMemoryBuffer>(
                size, msaa_sample_count, color_params, context_provider_wrapper,
                resource_dispatcher);
        break;
      case kRamGpuMemoryBufferResourceType:
        if (presentation_mode != kAllowImageChromiumPresentationMode)
          continue;
        if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(
                gfx::Size(size), color_params.GetBufferFormat())) {
          continue;
        }
        provider = std::make_unique<CanvasResourceProviderRamGpuMemoryBuffer>(
            size, color_params, resource_dispatcher);
        break;
      case kSharedBitmapResourceType:
        if (!IsBitmapFormatSupported(color_params.TransferableResourceFormat()))
          continue;
        if (!resource_dispatcher)
          continue;
        provider = std::make_unique<CanvasResourceProviderSharedBitmap>(
            size, color_params, resource_dispatcher);
        break;
      case kTextureResourceType:
        DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
        provider = std::make_unique<CanvasResourceProviderTexture>(
            size, msaa_sample_count, color_params, context_provider_wrapper,
            resource_dispatcher);
        break;
      case kBitmapResourceType:
        provider = std::make_unique<CanvasResourceProviderBitmap>(
            size, color_params, resource_dispatcher);
        break;
    }
    if (provider && provider->IsValid())
      return provider;
  }

  return nullptr;
}

CanvasResourceProvider::CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache,
    const gfx::ColorSpace& target_color_space)
    : playback_image_provider_(cache,
                               target_color_space,
                               cc::PlaybackImageProvider::Settings()) {}

CanvasResourceProvider::CanvasImageProvider::~CanvasImageProvider() = default;

cc::ImageProvider::ScopedDecodedDrawImage
CanvasResourceProvider::CanvasImageProvider::GetDecodedDrawImage(
    const cc::DrawImage& draw_image) {
  auto scoped_decoded_image =
      playback_image_provider_.GetDecodedDrawImage(draw_image);
  if (!scoped_decoded_image.needs_unlock())
    return scoped_decoded_image;

  if (!scoped_decoded_image.decoded_image().is_budgeted()) {
    // If we have exceeded the budget, ReleaseLockedImages any locked decodes.
    ReleaseLockedImages();
  }

  // It is safe to use base::Unretained, since decodes acquired from a provider
  // must not exceed the provider's lifetime.
  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedDecodedDrawImage(
      decoded_draw_image,
      base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                     base::Unretained(this), std::move(scoped_decoded_image)));
}

void CanvasResourceProvider::CanvasImageProvider::ReleaseLockedImages() {
  locked_images_.clear();
}

void CanvasResourceProvider::CanvasImageProvider::CanUnlockImage(
    ScopedDecodedDrawImage image) {
  locked_images_.push_back(std::move(image));
}

CanvasResourceProvider::CanvasResourceProvider(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
    : context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_dispatcher_(resource_dispatcher),
      size_(size),
      color_params_(color_params),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      weak_ptr_factory_(this) {
  if (context_provider_wrapper_)
    context_provider_wrapper_->AddObserver(this);
}

CanvasResourceProvider::~CanvasResourceProvider() {
  if (context_provider_wrapper_)
    context_provider_wrapper_->RemoveObserver(this);
}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_)
    surface_ = CreateSkSurface();
  return surface_.get();
}

PaintCanvas* CanvasResourceProvider::Canvas() {
  if (!canvas_) {
    TRACE_EVENT0("blink", "CanvasResourceProvider::Canvas");

    DCHECK(!canvas_image_provider_);

    gfx::ColorSpace target_color_space =
        ColorParams().NeedsSkColorSpaceXformCanvas()
            ? ColorParams().GetStorageGfxColorSpace()
            : gfx::ColorSpace::CreateSRGB();

    canvas_image_provider_.emplace(ImageDecodeCache(), target_color_space);
    cc::ImageProvider* image_provider = &*canvas_image_provider_;

    cc::SkiaPaintCanvas::ContextFlushes context_flushes;
    if (IsAccelerated() &&
        !ContextProviderWrapper()
             ->ContextProvider()
             ->GetGpuFeatureInfo()
             .IsWorkaroundEnabled(gpu::DISABLE_2D_CANVAS_AUTO_FLUSH)) {
      context_flushes.enable =
          CanvasHeuristicParameters::kEnableGrContextFlushes;
      context_flushes.max_draws_before_flush =
          CanvasHeuristicParameters::kMaxDrawsBeforeContextFlush;
    }
    if (ColorParams().NeedsSkColorSpaceXformCanvas()) {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), ColorParams().GetSkColorSpace(),
          image_provider, context_flushes);
    } else {
      canvas_ = std::make_unique<cc::SkiaPaintCanvas>(
          GetSkSurface()->getCanvas(), image_provider, context_flushes);
    }
  }

  return canvas_.get();
}

void CanvasResourceProvider::OnContextDestroyed() {
  if (canvas_image_provider_) {
    DCHECK(canvas_);
    canvas_->reset_image_provider();
    canvas_image_provider_.reset();
  }
}

void CanvasResourceProvider::ReleaseLockedImages() {
  if (canvas_image_provider_)
    canvas_image_provider_->ReleaseLockedImages();
}

scoped_refptr<StaticBitmapImage> CanvasResourceProvider::Snapshot() {
  if (!IsValid())
    return nullptr;

  auto sk_image = GetSkSurface()->makeImageSnapshot();
  auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
  snapshot_sk_image_id_ = sk_image->uniqueID();

  if (ContextProviderWrapper()) {
    return StaticBitmapImage::Create(std::move(sk_image),
                                     ContextProviderWrapper());
  }

  // Ensure that a new PaintImage::ContentId is used only when the underlying
  // SkImage changes. This is necessary to ensure that the same image results
  // in a cache hit in cc's ImageDecodeCache.
  if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
      last_snapshot_sk_image_id != snapshot_sk_image_id_) {
    snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
  }

  auto paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(snapshot_paint_image_id_)
          .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
          .TakePaintImage();
  return StaticBitmapImage::Create(std::move(paint_image));
}

gpu::gles2::GLES2Interface* CanvasResourceProvider::ContextGL() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ContextGL();
}

GrContext* CanvasResourceProvider::GetGrContext() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->GetGrContext();
}

void CanvasResourceProvider::FlushSkia() const {
  GetSkSurface()->flush();
}

bool CanvasResourceProvider::IsGpuContextLost() const {
  auto* gl = ContextGL();
  return !gl || gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool CanvasResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  TRACE_EVENT0("blink", "CanvasResourceProvider::WritePixels");

  DCHECK(IsValid());
  return GetSkSurface()->getCanvas()->writePixels(orig_info, pixels, row_bytes,
                                                  x, y);
}

void CanvasResourceProvider::Clear() {
  // Clear the background transparent or opaque, as required. It would be nice
  // if this wasn't required, but the canvas is currently filled with the magic
  // transparency color. Can we have another way to manage this?
  DCHECK(IsValid());
  if (color_params_.GetOpacityMode() == kOpaque)
    Canvas()->clear(SK_ColorBLACK);
  else
    Canvas()->clear(SK_ColorTRANSPARENT);
}

void CanvasResourceProvider::InvalidateSurface() {
  canvas_ = nullptr;
  canvas_image_provider_.reset();
  xform_canvas_ = nullptr;
  surface_ = nullptr;
}

uint32_t CanvasResourceProvider::ContentUniqueID() const {
  return GetSkSurface()->generationID();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::CreateResource() {
  // Needs to be implemented in subclasses that use resource recycling.
  NOTREACHED();
  return nullptr;
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCache() {
  if (context_provider_wrapper_)
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache();
  return &Image::SharedCCDecodeCache();
}

void CanvasResourceProvider::RecycleResource(
    scoped_refptr<CanvasResource> resource) {
  DCHECK(resource->HasOneRef());
  if (resource_recycling_enabled_)
    recycled_resources_.push_back(std::move(resource));
}

void CanvasResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_)
    ClearRecycledResources();
}

void CanvasResourceProvider::ClearRecycledResources() {
  recycled_resources_.clear();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::NewOrRecycledResource() {
  if (recycled_resources_.size()) {
    scoped_refptr<CanvasResource> resource =
        std::move(recycled_resources_.back());
    recycled_resources_.pop_back();
    return resource;
  }
  return CreateResource();
}

}  // namespace blink
