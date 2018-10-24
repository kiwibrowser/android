// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SCENIC_SESSION_H_
#define UI_OZONE_PLATFORM_SCENIC_SCENIC_SESSION_H_

#include <string>
#include <vector>

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/macros.h"
#include "base/memory/shared_memory_handle.h"

namespace ui {

// Interface used to receive events and error notifications from ScenicSession.
class ScenicSessionListener {
 public:
  ScenicSessionListener() = default;

  virtual void OnScenicError(const std::string& error) = 0;
  virtual void OnScenicEvents(
      const std::vector<fuchsia::ui::scenic::Event>& events) = 0;

 protected:
  virtual ~ScenicSessionListener() = default;

  DISALLOW_COPY_AND_ASSIGN(ScenicSessionListener);
};

// ScenicSession represents a session used to interact with Scenic. It sends
// commands to Scenic via fuchsia::ui::scenic::Session interface. ScenicWindow
// creates a separate session for each window.
class ScenicSession : public fuchsia::ui::scenic::SessionListener {
 public:
  using ResourceId = uint32_t;

  // Creates and wraps a new session for |scenic_service|. |listener| must
  // outlive ScenicSession.
  ScenicSession(fuchsia::ui::scenic::Scenic* scenic_service,
                ScenicSessionListener* listener);

  ~ScenicSession() override;

  // Following functions enqueue various Scenic commands. See
  // https://fuchsia.googlesource.com/garnet/+/master/public/lib/ui/gfx/fidl/commands.fidl
  // for descriptions of the commands and the fields. Create*() methods return
  // ID of the created resource. Resource IDs are only valid within context of
  // the session.
  void ReleaseResource(ResourceId resource_id);
  ResourceId CreateMemory(base::SharedMemoryHandle vmo,
                          fuchsia::images::MemoryType memory_type);
  ResourceId CreateImage(ResourceId memory_id,
                         ResourceId memory_offset,
                         fuchsia::images::ImageInfo info);
  ResourceId ImportResource(fuchsia::ui::gfx::ImportSpec spec,
                            zx::eventpair import_token);
  ResourceId CreateEntityNode();
  ResourceId CreateShapeNode();
  void AddNodeChild(ResourceId node_id, ResourceId child_id);
  void SetNodeTranslation(ResourceId node_id, const float translation[3]);
  ResourceId CreateRectangle(float width, float height);
  ResourceId CreateMaterial();
  void SetNodeMaterial(ResourceId node_id, ResourceId material_id);
  void SetNodeShape(ResourceId node_id, ResourceId shape_id);
  void SetMaterialTexture(ResourceId material_id, ResourceId texture_id);
  void SetEventMask(ResourceId resource_id, uint32_t event_mask);

  // Flushes queued commands and presents the resulting frame.
  void Present();

 private:
  // fuchsia::ui::scenic::SessionListener interface.
  void OnError(fidl::StringPtr error) override;
  void OnEvent(fidl::VectorPtr<fuchsia::ui::scenic::Event> events) override;

  // Allocates a new unique resource id.
  ResourceId AllocateResourceId();

  // Enqueues a scenic |command|.
  void EnqueueGfxCommand(fuchsia::ui::gfx::Command command);

  // Flushes |queued_commands_|.
  void Flush();

  // Unbinds |session_| and |session_listener_binding_|, which frees the
  // resources used for the session in Scenic and guarantees that we wont
  // receive SessionListener events.
  void Close();

  ScenicSessionListener* const listener_;

  fuchsia::ui::scenic::SessionPtr session_;
  fidl::Binding<fuchsia::ui::scenic::SessionListener> session_listener_binding_;

  ResourceId next_resource_id_ = 1u;

  // Used to verify that all resources are freed when the session is closed.
  int resource_count_ = 0u;

  fidl::VectorPtr<fuchsia::ui::scenic::Command> queued_commands_;

  DISALLOW_COPY_AND_ASSIGN(ScenicSession);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SCENIC_SESSION_H_
