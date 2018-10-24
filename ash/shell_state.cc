// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_state.h"

#include <memory>
#include <utility>

#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace ash {

ShellState::ShellState() = default;

ShellState::~ShellState() = default;

void ShellState::BindRequest(mojom::ShellStateRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

aura::Window* ShellState::GetRootWindowForNewWindows() const {
  if (scoped_root_window_for_new_windows_)
    return scoped_root_window_for_new_windows_;
  return root_window_for_new_windows_;
}

void ShellState::SetRootWindowForNewWindows(aura::Window* root) {
  if (root == root_window_for_new_windows_)
    return;
  root_window_for_new_windows_ = root;
  NotifyAllClients();
}

void ShellState::AddClient(mojom::ShellStateClientPtr client) {
  mojom::ShellStateClient* client_impl = client.get();
  clients_.AddPtr(std::move(client));
  client_impl->SetDisplayIdForNewWindows(GetDisplayIdForNewWindows());
}

void ShellState::FlushMojoForTest() {
  clients_.FlushForTesting();
}

void ShellState::NotifyAllClients() {
  const int64_t display_id = GetDisplayIdForNewWindows();
  clients_.ForAllPtrs([display_id](mojom::ShellStateClient* client) {
    client->SetDisplayIdForNewWindows(display_id);
  });
}

int64_t ShellState::GetDisplayIdForNewWindows() const {
  // GetDisplayNearestWindow() handles null.
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(GetRootWindowForNewWindows())
      .id();
}

void ShellState::SetScopedRootWindowForNewWindows(aura::Window* root) {
  if (root == scoped_root_window_for_new_windows_)
    return;
  // Only allow set and clear, not switch.
  DCHECK(!scoped_root_window_for_new_windows_ || !root);
  scoped_root_window_for_new_windows_ = root;
  NotifyAllClients();
}

}  // namespace ash
