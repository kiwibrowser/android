// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_state.h"

#include <stdint.h>

#include "ash/public/interfaces/shell_state.mojom.h"
#include "ash/scoped_root_window_for_new_windows.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/manager/display_manager.h"

namespace ash {
namespace {

// Simulates the client interface in chrome.
class TestShellStateClient : public mojom::ShellStateClient {
 public:
  TestShellStateClient() = default;
  ~TestShellStateClient() override = default;

  mojom::ShellStateClientPtr CreateInterfacePtrAndBind() {
    mojom::ShellStateClientPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // mojom::ShellStateClient:
  void SetDisplayIdForNewWindows(int64_t display_id) override {
    last_display_id_ = display_id;
  }

  int64_t last_display_id_ = 0;

 private:
  mojo::Binding<mojom::ShellStateClient> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestShellStateClient);
};

using ShellStateTest = AshTestBase;

TEST_F(ShellStateTest, Basics) {
  UpdateDisplay("1024x768,800x600");
  const int64_t primary_display_id = display_manager()->GetDisplayAt(0).id();
  const int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();

  ShellState* shell_state = Shell::Get()->shell_state();
  TestShellStateClient client;

  // Adding a client notifies it with the initial display id.
  shell_state->AddClient(client.CreateInterfacePtrAndBind());
  shell_state->FlushMojoForTest();
  EXPECT_EQ(primary_display_id, client.last_display_id_);

  // Setting a root window for new windows notifies the client.
  ScopedRootWindowForNewWindows scoped_root(Shell::GetAllRootWindows()[1]);
  shell_state->FlushMojoForTest();
  EXPECT_EQ(secondary_display_id, client.last_display_id_);
}

}  // namespace
}  // namespace ash
