// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELL_STATE_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_SHELL_STATE_CLIENT_H_

#include <memory>

#include "ash/public/interfaces/shell_state.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Caches ash::Shell state. The initial values are loaded asynchronously at
// startup because we don't want Chrome to block on startup waiting for Ash.
class ShellStateClient : public ash::mojom::ShellStateClient {
 public:
  ShellStateClient();
  ~ShellStateClient() override;

  // Initializes and connects to ash.
  void Init();

  // Tests can provide a mock mojo interface for the ash interface.
  void InitForTesting(ash::mojom::ShellStatePtr shell_state_ptr);

  static ShellStateClient* Get();

  int64_t display_id_for_new_windows() const {
    return display_id_for_new_windows_;
  }

  // ash::mojom::ShellStateClient:
  void SetDisplayIdForNewWindows(int64_t display_id) override;

  // Flushes the mojo pipe to ash.
  void FlushForTesting();

 private:
  friend class ScopedDisplayIdForNewWindows;

  // Binds this object to its mojo interface and registers it as an ash client.
  void BindAndAddClient();

  // The mojo interface in ash.
  ash::mojom::ShellStatePtr shell_state_ptr_;

  // Binds to the observer interface from ash.
  mojo::Binding<ash::mojom::ShellStateClient> binding_;

  int64_t display_id_for_new_windows_;

  DISALLOW_COPY_AND_ASSIGN(ShellStateClient);
};

#endif  // CHROME_BROWSER_UI_ASH_SHELL_STATE_CLIENT_H_
