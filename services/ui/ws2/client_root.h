// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_CLIENT_ROOT_H_
#define SERVICES_UI_WS2_CLIENT_ROOT_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class ClientSurfaceEmbedder;
class Window;
}  // namespace aura

namespace ui {
namespace ws2 {

class WindowHostFrameSinkClient;
class WindowTree;

// WindowTree creates a ClientRoot for each window the client is embedded in. A
// ClientRoot is created as the result of another client using Embed(), or this
// client requesting a top-level window. ClientRoot is responsible for
// maintaining state associated with the root, as well as notifying the client
// of any changes to the root Window.
class COMPONENT_EXPORT(WINDOW_SERVICE) ClientRoot
    : public aura::WindowObserver {
 public:
  ClientRoot(WindowTree* window_tree, aura::Window* window, bool is_top_level);
  ~ClientRoot() override;

  // Registers the necessary state needed for embedding in viz.
  void RegisterVizEmbeddingSupport();

  aura::Window* window() { return window_; }

  bool is_top_level() const { return is_top_level_; }

 private:
  void UpdatePrimarySurfaceId();

  // Returns true if the WindowService should assign the LocalSurfaceId. A value
  // of false means the client is expected to providate the LocalSurfaceId.
  bool ShouldAssignLocalSurfaceId();

  // If necessary, this updates the LocalSurfaceId.
  void UpdateLocalSurfaceIdIfNecessary();

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  WindowTree* window_tree_;
  aura::Window* window_;
  const bool is_top_level_;

  // |last_surface_size_in_pixels_| and |last_device_scale_factor_| are only
  // used if a LocalSurfaceId is needed for the window. They represent the size
  // and device scale factor at the time the LocalSurfaceId was generated.
  gfx::Size last_surface_size_in_pixels_;
  float last_device_scale_factor_ = 1.0f;
  viz::ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;

  std::unique_ptr<aura::ClientSurfaceEmbedder> client_surface_embedder_;
  // viz::HostFrameSinkClient registered with the HostFrameSinkManager for the
  // window.
  std::unique_ptr<WindowHostFrameSinkClient> window_host_frame_sink_client_;

  DISALLOW_COPY_AND_ASSIGN(ClientRoot);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_CLIENT_ROOT_H_
