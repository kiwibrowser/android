// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shell_state_client.h"

#include "ash/public/interfaces/shell_state.mojom.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestShellState : ash::mojom::ShellState {
 public:
  TestShellState() : binding_(this) {}
  ~TestShellState() override = default;

  ash::mojom::ShellStatePtr CreateInterfacePtr() {
    ash::mojom::ShellStatePtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // ash::mojom::ShellState:
  void AddClient(ash::mojom::ShellStateClientPtr client) override {
    ++add_client_count_;
  }

  int add_client_count() const { return add_client_count_; }

 private:
  mojo::Binding<ash::mojom::ShellState> binding_;
  int add_client_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShellState);
};

TEST(ShellStateClientTest, Basics) {
  base::test::ScopedTaskEnvironment scoped_task_enviroment;
  ShellStateClient client;
  TestShellState ash_shell_state;
  client.InitForTesting(ash_shell_state.CreateInterfacePtr());
  client.FlushForTesting();

  // Client was added to ash.
  EXPECT_TRUE(ash_shell_state.add_client_count());

  client.SetDisplayIdForNewWindows(123);
  EXPECT_EQ(123, client.display_id_for_new_windows());
}

}  // namespace
