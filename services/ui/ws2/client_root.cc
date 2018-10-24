// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/client_root.h"

#include "components/viz/host/host_frame_sink_manager.h"
#include "services/ui/ws2/client_change.h"
#include "services/ui/ws2/client_change_tracker.h"
#include "services/ui/ws2/server_window.h"
#include "services/ui/ws2/window_host_frame_sink_client.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_tree.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/client_surface_embedder.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/dip_util.h"

namespace ui {
namespace ws2 {

ClientRoot::ClientRoot(WindowTree* window_tree,
                       aura::Window* window,
                       bool is_top_level)
    : window_tree_(window_tree), window_(window), is_top_level_(is_top_level) {
  window_->AddObserver(this);
  // TODO: wire up gfx::Insets() correctly below. See usage in
  // aura::ClientSurfaceEmbedder for details. Insets here are used for
  // guttering.
  client_surface_embedder_ = std::make_unique<aura::ClientSurfaceEmbedder>(
      window_, is_top_level, gfx::Insets());
  // Ensure there is a valid LocalSurfaceId (if necessary).
  UpdateLocalSurfaceIdIfNecessary();
}

ClientRoot::~ClientRoot() {
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window_);
  window_->RemoveObserver(this);

  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  host_frame_sink_manager->InvalidateFrameSinkId(
      server_window->frame_sink_id());
}

void ClientRoot::RegisterVizEmbeddingSupport() {
  // This function should only be called once.
  DCHECK(!window_host_frame_sink_client_);
  window_host_frame_sink_client_ = std::make_unique<WindowHostFrameSinkClient>(
      client_surface_embedder_.get());

  viz::HostFrameSinkManager* host_frame_sink_manager =
      aura::Env::GetInstance()
          ->context_factory_private()
          ->GetHostFrameSinkManager();
  viz::FrameSinkId frame_sink_id =
      ServerWindow::GetMayBeNull(window_)->frame_sink_id();
  host_frame_sink_manager->RegisterFrameSinkId(
      frame_sink_id, window_host_frame_sink_client_.get());
  window_->SetEmbedFrameSinkId(frame_sink_id);

  UpdatePrimarySurfaceId();
}

bool ClientRoot::ShouldAssignLocalSurfaceId() {
  // First level embeddings have their LocalSurfaceId assigned by the
  // WindowService. First level embeddings have no embeddings above them.
  if (is_top_level_)
    return true;
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window_);
  return server_window->owning_window_tree() == nullptr;
}

void ClientRoot::UpdateLocalSurfaceIdIfNecessary() {
  if (!ShouldAssignLocalSurfaceId())
    return;

  gfx::Size size_in_pixels =
      ui::ConvertSizeToPixel(window_->layer(), window_->bounds().size());
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window_);
  // It's expected by cc code that any time the size changes a new
  // LocalSurfaceId is used.
  if (last_surface_size_in_pixels_ != size_in_pixels ||
      !server_window->local_surface_id().has_value() ||
      !server_window->local_surface_id()->is_valid() ||
      last_device_scale_factor_ != window_->layer()->device_scale_factor()) {
    server_window->set_local_surface_id(
        parent_local_surface_id_allocator_.GenerateId());
    last_surface_size_in_pixels_ = size_in_pixels;
    last_device_scale_factor_ = window_->layer()->device_scale_factor();
  }
}

void ClientRoot::UpdatePrimarySurfaceId() {
  UpdateLocalSurfaceIdIfNecessary();
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window_);
  if (server_window->local_surface_id().has_value()) {
    client_surface_embedder_->SetPrimarySurfaceId(viz::SurfaceId(
        window_->GetFrameSinkId(), *server_window->local_surface_id()));
  }
}

void ClientRoot::OnWindowPropertyChanged(aura::Window* window,
                                         const void* key,
                                         intptr_t old) {
  if (window_tree_->property_change_tracker_->IsProcessingChangeForWindow(
          window, ClientChangeType::kProperty)) {
    // Do not send notifications for changes intiated by the client.
    return;
  }
  std::string transport_name;
  std::unique_ptr<std::vector<uint8_t>> transport_value;
  if (window_tree_->window_service()
          ->property_converter()
          ->ConvertPropertyForTransport(window, key, &transport_name,
                                        &transport_value)) {
    base::Optional<std::vector<uint8_t>> transport_value_mojo;
    if (transport_value)
      transport_value_mojo.emplace(std::move(*transport_value));
    window_tree_->window_tree_client_->OnWindowSharedPropertyChanged(
        window_tree_->TransportIdForWindow(window), transport_name,
        transport_value_mojo);
  }
}

void ClientRoot::OnWindowBoundsChanged(aura::Window* window,
                                       const gfx::Rect& old_bounds,
                                       const gfx::Rect& new_bounds,
                                       ui::PropertyChangeReason reason) {
  UpdatePrimarySurfaceId();
  client_surface_embedder_->UpdateSizeAndGutters();
  // See comments in WindowTree::SetWindowBoundsImpl() for details on
  // why this always notifies the client.
  window_tree_->window_tree_client_->OnWindowBoundsChanged(
      window_tree_->TransportIdForWindow(window), old_bounds, new_bounds,
      ServerWindow::GetMayBeNull(window_)->local_surface_id());
}

}  // namespace ws2
}  // namespace ui
