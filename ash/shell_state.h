// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_STATE_H_
#define ASH_SHELL_STATE_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/interfaces/shell_state.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace aura {
class Window;
}

namespace ash {

// Provides access via mojo to ash::Shell state.
class ASH_EXPORT ShellState : public mojom::ShellState {
 public:
  ShellState();
  ~ShellState() override;

  // Binds the mojom::ShellState interface to this object.
  void BindRequest(mojom::ShellStateRequest request);

  // Returns the root window that newly created windows should be added to.
  // Value can be temporarily overridden using ScopedRootWindowForNewWindows.
  // NOTE: This returns the root; newly created windows should be added to the
  // appropriate container in the returned window.
  aura::Window* GetRootWindowForNewWindows() const;

  // Updates the root window and notifies observers.
  // NOTE: Prefer ScopedRootWindowForNewWindows.
  void SetRootWindowForNewWindows(aura::Window* root);

  // mojom::ShellState:
  void AddClient(mojom::ShellStateClientPtr client) override;

  void FlushMojoForTest();

 private:
  friend class ScopedRootWindowForNewWindows;

  // Sends a state update to all clients.
  void NotifyAllClients();

  int64_t GetDisplayIdForNewWindows() const;

  // Sets the value and updates clients.
  void SetScopedRootWindowForNewWindows(aura::Window* root);

  // Binding for mojom::ShellState interface.
  mojo::BindingSet<mojom::ShellState> bindings_;

  // Clients (e.g. chrome browser, other mojo apps).
  mojo::InterfacePtrSet<mojom::ShellStateClient> clients_;

  aura::Window* root_window_for_new_windows_ = nullptr;

  // See ScopedRootWindowForNewWindows.
  aura::Window* scoped_root_window_for_new_windows_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellState);
};

}  // namespace ash

#endif  // ASH_SHELL_STATE_H_
