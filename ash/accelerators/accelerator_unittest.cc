// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"
#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "services/ui/ws2/test_window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"

namespace ash {
namespace accelerators {

using AcceleratorTest = AshTestBase;

// This is meant to exercise an end to end test of an accelerator that happens
// *after* the remote client is given a chance to handle it.
TEST_F(AcceleratorTest, PostAcceleratorWorks) {
  // Register a post-accelerator. That is, an accelerator that is handled
  // *after* the remote client (focused target) is given a chance.
  ui::TestAcceleratorTarget test_target;
  const ui::KeyboardCode accelerator_code = ui::VKEY_N;
  const int accelerator_modifiers = ui::EF_CONTROL_DOWN;
  Shell::Get()->accelerator_controller()->Register(
      {ui::Accelerator(accelerator_code, accelerator_modifiers)}, &test_target);
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window->Focus();
  ASSERT_TRUE(window->HasFocus());
  GetEventGenerator().PressKey(accelerator_code, accelerator_modifiers);

  // The accelerator was not pressed yet (the KeyEvent was sent to the client,
  // but the client hasn't responded).
  EXPECT_EQ(0, test_target.accelerator_count());

  EXPECT_TRUE(GetTestWindowTreeClient()->AckFirstEvent(
      GetWindowTree(), ui::mojom::EventResult::UNHANDLED));

  // The client didn't handle the event, so |test_target| should get the
  // accelerator.
  EXPECT_EQ(1, test_target.accelerator_count());
}

}  // namespace accelerators
}  // namespace ash
