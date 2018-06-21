// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_resource_provider.h"

#include "base/bits.h"
#include "base/debug/stack_trace.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace viz {

struct ClientResourceProvider::ImportedResource {
  TransferableResource resource;
  std::unique_ptr<SingleReleaseCallback> release_callback;
  int exported_count = 0;
  bool marked_for_deletion = false;

  gpu::SyncToken returned_sync_token;
  bool returned_lost = false;

#if DCHECK_IS_ON()
  base::debug::StackTrace stack_trace;
#endif

  ImportedResource(ResourceId id,
                   const TransferableResource& resource,
                   std::unique_ptr<SingleReleaseCallback> release_callback)
      : resource(resource),
        release_callback(std::move(release_callback)),
        // If the resource is immediately deleted, it returns the same SyncToken
        // it came with. The client may need to wait on that before deleting the
        // backing or reusing it.
        returned_sync_token(resource.mailbox_holder.sync_token) {
    // Replace the |resource| id with the local id from this
    // ClientResourceProvider.
    this->resource.id = id;
  }
  ~ImportedResource() = default;

  ImportedResource(ImportedResource&&) = default;
  ImportedResource& operator=(ImportedResource&&) = default;
};

ClientResourceProvider::ClientResourceProvider(
    bool delegated_sync_points_required)
    : delegated_sync_points_required_(delegated_sync_points_required) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

ClientResourceProvider::~ClientResourceProvider() {
  // If this fails, there are outstanding resources exported that should be
  // lost and returned by calling ShutdownAndReleaseAllResources(), or there
  // are resources that were imported without being removed by
  // RemoveImportedResource(). In either case, calling
  // ShutdownAndReleaseAllResources() will help, as it will report which
  // resources were imported without being removed as well.
  DCHECK(imported_resources_.empty());
}

gpu::SyncToken ClientResourceProvider::GenerateSyncTokenHelper(
    gpu::gles2::GLES2Interface* gl) {
  DCHECK(gl);
  gpu::SyncToken sync_token;
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  DCHECK(sync_token.HasData() ||
         gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  return sync_token;
}

gpu::SyncToken ClientResourceProvider::GenerateSyncTokenHelper(
    gpu::raster::RasterInterface* ri) {
  DCHECK(ri);
  gpu::SyncToken sync_token;
  ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
  DCHECK(sync_token.HasData() ||
         ri->GetGraphicsResetStatusKHR() != GL_NO_ERROR);
  return sync_token;
}

void ClientResourceProvider::PrepareSendToParent(
    const std::vector<ResourceId>& export_ids,
    std::vector<TransferableResource>* list,
    ContextProvider* context_provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This function goes through the array multiple times, store the resources
  // as pointers so we don't have to look up the resource id multiple times.
  // Make sure the maps do not change while these vectors are alive or they
  // will become invalid.
  std::vector<ImportedResource*> imports;
  imports.reserve(export_ids.size());
  for (const ResourceId id : export_ids) {
    auto it = imported_resources_.find(id);
    DCHECK(it != imported_resources_.end());
    imports.push_back(&it->second);
  }

  // Lazily create any mailboxes and verify all unverified sync tokens.
  std::vector<GLbyte*> unverified_sync_tokens;
  if (delegated_sync_points_required_) {
    for (ImportedResource* imported : imports) {
      if (!imported->resource.is_software &&
          !imported->resource.mailbox_holder.sync_token.verified_flush()) {
        unverified_sync_tokens.push_back(
            imported->resource.mailbox_holder.sync_token.GetData());
      }
    }
  }

  if (!unverified_sync_tokens.empty()) {
    DCHECK(delegated_sync_points_required_);
    DCHECK(context_provider);
    context_provider->ContextGL()->VerifySyncTokensCHROMIUM(
        unverified_sync_tokens.data(), unverified_sync_tokens.size());
  }

  for (ImportedResource* imported : imports) {
    list->push_back(imported->resource);
    imported->exported_count++;
  }
}

void ClientResourceProvider::ReceiveReturnsFromParent(
    const std::vector<ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const ReturnedResource& returned : resources) {
    ResourceId local_id = returned.id;

    auto import_it = imported_resources_.find(local_id);
    DCHECK(import_it != imported_resources_.end());
    ImportedResource& imported = import_it->second;

    DCHECK_GE(imported.exported_count, returned.count);
    imported.exported_count -= returned.count;
    imported.returned_lost |= returned.lost;

    if (imported.exported_count)
      continue;

    if (returned.sync_token.HasData()) {
      DCHECK(!imported.resource.is_software);
      imported.returned_sync_token = returned.sync_token;
    }

    if (imported.marked_for_deletion) {
      imported.release_callback->Run(imported.returned_sync_token,
                                     imported.returned_lost);
      imported_resources_.erase(import_it);
    }
  }
}

ResourceId ClientResourceProvider::ImportResource(
    const TransferableResource& resource,
    std::unique_ptr<SingleReleaseCallback> release_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ResourceId id = next_id_++;
  auto result = imported_resources_.emplace(
      id, ImportedResource(id, resource, std::move(release_callback)));
  DCHECK(result.second);  // If false, the id was already in the map.
  return id;
}

void ClientResourceProvider::RemoveImportedResource(ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = imported_resources_.find(id);
  DCHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  imported.marked_for_deletion = true;
  if (imported.exported_count == 0) {
    imported.release_callback->Run(imported.returned_sync_token,
                                   imported.returned_lost);
    imported_resources_.erase(it);
  }
}

void ClientResourceProvider::ReleaseAllExportedResources(bool lose) {
  std::vector<ResourceId> to_remove;
  for (auto& pair : imported_resources_) {
    ImportedResource& imported = pair.second;
    if (!imported.exported_count)
      continue;
    imported.exported_count = 0;
    imported.returned_lost |= lose;
    if (imported.marked_for_deletion) {
      imported.release_callback->Run(imported.returned_sync_token,
                                     imported.returned_lost);
      to_remove.push_back(pair.first);
    }
  }
  for (ResourceId id : to_remove)
    imported_resources_.erase(id);
}

void ClientResourceProvider::ShutdownAndReleaseAllResources() {
  for (auto& pair : imported_resources_) {
    ImportedResource& imported = pair.second;

#if DCHECK_IS_ON()
    // If this is false, then the resource has not been removed via
    // RemoveImportedResource(), and all resources should be removed before
    // we resort to marking resources as lost during shutdown.
    DCHECK(imported.marked_for_deletion)
        << "id: " << pair.first << " from:\n"
        << imported.stack_trace.ToString() << "===";
    DCHECK(imported.exported_count) << "id: " << pair.first << " from:\n"
                                    << imported.stack_trace.ToString() << "===";
#endif

    imported.release_callback->Run(imported.returned_sync_token,
                                   /*is_lost=*/true);
  }
  imported_resources_.clear();
}

ClientResourceProvider::ScopedSkSurface::ScopedSkSurface(
    GrContext* gr_context,
    GLuint texture_id,
    GLenum texture_target,
    const gfx::Size& size,
    ResourceFormat format,
    bool can_use_lcd_text,
    int msaa_sample_count) {
  GrGLTextureInfo texture_info;
  texture_info.fID = texture_id;
  texture_info.fTarget = texture_target;
  texture_info.fFormat = TextureStorageFormat(format);
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   GrMipMapped::kNo, texture_info);
  SkSurfaceProps surface_props = ComputeSurfaceProps(can_use_lcd_text);
  // This type is used only for gpu raster, which implies gpu compositing.
  bool gpu_compositing = true;
  surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
      gr_context, backend_texture, kTopLeft_GrSurfaceOrigin, msaa_sample_count,
      ResourceFormatToClosestSkColorType(gpu_compositing, format), nullptr,
      &surface_props);
}

ClientResourceProvider::ScopedSkSurface::~ScopedSkSurface() {
  if (surface_)
    surface_->prepareForExternalIO();
}

SkSurfaceProps ClientResourceProvider::ScopedSkSurface::ComputeSurfaceProps(
    bool can_use_lcd_text) {
  uint32_t flags = 0;
  // Use unknown pixel geometry to disable LCD text.
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  return surface_props;
}

void ClientResourceProvider::ValidateResource(ResourceId id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  DCHECK(imported_resources_.find(id) != imported_resources_.end());
}

bool ClientResourceProvider::InUseByConsumer(ResourceId id) {
  auto it = imported_resources_.find(id);
  DCHECK(it != imported_resources_.end());
  ImportedResource& imported = it->second;
  return imported.exported_count > 0 || imported.returned_lost;
}

size_t ClientResourceProvider::num_resources_for_testing() const {
  return imported_resources_.size();
}

}  // namespace viz
