// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_tree_test_helper.h"

#include "services/ui/ws2/server_window.h"
#include "services/ui/ws2/window_tree.h"
#include "services/ui/ws2/window_tree_binding.h"

namespace ui {
namespace ws2 {

WindowTreeTestHelper::WindowTreeTestHelper(WindowTree* window_tree)
    : window_tree_(window_tree) {}

WindowTreeTestHelper::~WindowTreeTestHelper() = default;

mojom::WindowTree* WindowTreeTestHelper::window_tree() {
  return static_cast<mojom::WindowTree*>(window_tree_);
}

mojom::WindowDataPtr WindowTreeTestHelper::WindowToWindowData(
    aura::Window* window) {
  return window_tree_->WindowToWindowData(window);
}

aura::Window* WindowTreeTestHelper::NewWindow(
    Id transport_window_id,
    base::flat_map<std::string, std::vector<uint8_t>> properties) {
  if (transport_window_id == 0)
    transport_window_id = next_window_id_++;
  const uint32_t change_id = 1u;
  window_tree_->NewWindow(change_id, transport_window_id, properties);
  return window_tree_->GetWindowByClientId(
      window_tree_->MakeClientWindowId(transport_window_id));
}

void WindowTreeTestHelper::DeleteWindow(aura::Window* window) {
  const int change_id = 1u;
  window_tree_->DeleteWindow(change_id, TransportIdForWindow(window));
}

aura::Window* WindowTreeTestHelper::NewTopLevelWindow(
    Id transport_window_id,
    base::flat_map<std::string, std::vector<uint8_t>> properties) {
  if (transport_window_id == 0)
    transport_window_id = next_window_id_++;
  const uint32_t change_id = 1u;
  window_tree_->NewTopLevelWindow(change_id, transport_window_id, properties);
  return window_tree_->GetWindowByClientId(
      window_tree_->MakeClientWindowId(transport_window_id));
}

aura::Window* WindowTreeTestHelper::NewTopLevelWindow(
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  return NewTopLevelWindow(0u, properties);
}

bool WindowTreeTestHelper::SetCapture(aura::Window* window) {
  return window_tree_->SetCaptureImpl(ClientWindowIdForWindow(window));
}

bool WindowTreeTestHelper::ReleaseCapture(aura::Window* window) {
  return window_tree_->ReleaseCaptureImpl(ClientWindowIdForWindow(window));
}

bool WindowTreeTestHelper::ReorderWindow(aura::Window* window,
                                         aura::Window* relative_window,
                                         mojom::OrderDirection direction) {
  return window_tree_->ReorderWindowImpl(
      ClientWindowIdForWindow(window), ClientWindowIdForWindow(relative_window),
      direction);
}

bool WindowTreeTestHelper::SetWindowBounds(
    aura::Window* window,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  return window_tree_->SetWindowBoundsImpl(ClientWindowIdForWindow(window),
                                           bounds, local_surface_id);
}

void WindowTreeTestHelper::SetWindowBoundsWithAck(aura::Window* window,
                                                  const gfx::Rect& bounds,
                                                  uint32_t change_id) {
  base::Optional<viz::LocalSurfaceId> local_surface_id;
  window_tree_->SetWindowBounds(change_id, TransportIdForWindow(window), bounds,
                                local_surface_id);
}

void WindowTreeTestHelper::SetClientArea(
    aura::Window* window,
    const gfx::Insets& insets,
    base::Optional<std::vector<gfx::Rect>> additional_client_areas) {
  window_tree_->SetClientArea(TransportIdForWindow(window), insets,
                              additional_client_areas);
}

void WindowTreeTestHelper::SetWindowProperty(aura::Window* window,
                                             const std::string& name,
                                             const std::vector<uint8_t>& value,
                                             uint32_t change_id) {
  window_tree_->SetWindowProperty(change_id, TransportIdForWindow(window), name,
                                  value);
}

Embedding* WindowTreeTestHelper::Embed(aura::Window* window,
                                       mojom::WindowTreeClientPtr client_ptr,
                                       mojom::WindowTreeClient* client,
                                       uint32_t embed_flags) {
  if (!window_tree_->EmbedImpl(ClientWindowIdForWindow(window),
                               std::move(client_ptr), client, embed_flags)) {
    return nullptr;
  }
  return ServerWindow::GetMayBeNull(window)->embedding();
}

void WindowTreeTestHelper::SetEventTargetingPolicy(
    aura::Window* window,
    mojom::EventTargetingPolicy policy) {
  window_tree_->SetEventTargetingPolicy(TransportIdForWindow(window), policy);
}

void WindowTreeTestHelper::OnWindowInputEventAck(uint32_t event_id,
                                                 mojom::EventResult result) {
  window_tree_->OnWindowInputEventAck(event_id, result);
}

bool WindowTreeTestHelper::StackAbove(aura::Window* above_window,
                                      aura::Window* below_window) {
  return window_tree_->StackAboveImpl(ClientWindowIdForWindow(above_window),
                                      ClientWindowIdForWindow(below_window));
}

bool WindowTreeTestHelper::StackAtTop(aura::Window* window) {
  return window_tree_->StackAtTopImpl(ClientWindowIdForWindow(window));
}

Id WindowTreeTestHelper::TransportIdForWindow(aura::Window* window) {
  return window ? window_tree_->TransportIdForWindow(window)
                : kInvalidTransportId;
}

bool WindowTreeTestHelper::SetFocus(aura::Window* window) {
  return window_tree_->SetFocusImpl(ClientWindowIdForWindow(window));
}

void WindowTreeTestHelper::SetCanFocus(aura::Window* window, bool can_focus) {
  window_tree_->SetCanFocus(window_tree_->TransportIdForWindow(window),
                            can_focus);
}

void WindowTreeTestHelper::SetCursor(aura::Window* window,
                                     ui::CursorData cursor) {
  window_tree_->SetCursorImpl(ClientWindowIdForWindow(window), cursor);
}

void WindowTreeTestHelper::DestroyEmbedding(Embedding* embedding) {
  // Triggers WindowTree deleting the Embedding.
  window_tree_->OnEmbeddedClientConnectionLost(embedding);
}

ClientWindowId WindowTreeTestHelper::ClientWindowIdForWindow(
    aura::Window* window) {
  return window_tree_->MakeClientWindowId(TransportIdForWindow(window));
}

}  // namespace ws2
}  // namespace ui
