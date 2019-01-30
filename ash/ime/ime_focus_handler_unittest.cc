// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/ime_focus_handler.h"

#include <memory>

#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/wm/core/focus_controller.h"

namespace ash {

namespace {

// A testing input method that tracks OnFocus/OnBlur calls.
class TestInputMethod : public ui::MockInputMethod {
 public:
  explicit TestInputMethod(bool initial_focused)
      : MockInputMethod(nullptr), focused_(initial_focused) {}
  ~TestInputMethod() override = default;

  bool focused() const { return focused_; }

 private:
  // ui::MokcInputMethod
  void OnFocus() override { focused_ = true; }
  void OnBlur() override { focused_ = false; }

  bool focused_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethod);
};

}  // namespace

class ImeFocusHandlerTest : public AshTestBase {
 public:
  ImeFocusHandlerTest() = default;
  ~ImeFocusHandlerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    aura::client::FocusClient* const focus_controller =
        Shell::Get()->focus_controller();

    ime_focus_handler_ =
        std::make_unique<ImeFocusHandler>(focus_controller, &input_method_);
  }
  void TearDown() override {
    ime_focus_handler_.reset();

    AshTestBase::TearDown();
  }

  // Simulates a window created by a window service client.
  std::unique_ptr<aura::Window> CreateRemoteWindow() {
    return CreateTestWindow(gfx::Rect(0, 0, 100, 50));
  }

  TestInputMethod& input_method() { return input_method_; }

 private:
  TestInputMethod input_method_{true /* initial_focus */};
  std::unique_ptr<ImeFocusHandler> ime_focus_handler_;

  DISALLOW_COPY_AND_ASSIGN(ImeFocusHandlerTest);
};

// Tests that IME focus state is updated when the active window changes between
// a ClientWindow and an ash window.
TEST_F(ImeFocusHandlerTest, BetweenClientWindowAndAshWindow) {
  // This test relies on state only set in classic.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  // Activates a non-ash window. IME should lose focus.
  std::unique_ptr<aura::Window> non_ash_window = CreateRemoteWindow();
  wm::ActivateWindow(non_ash_window.get());
  EXPECT_FALSE(input_method().focused());

  // Activates an ash window. IME should gain focus.
  std::unique_ptr<aura::Window> ash_window(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 50)));
  wm::ActivateWindow(ash_window.get());
  EXPECT_TRUE(input_method().focused());

  // Activates a non-ash window again. IME should lose focus again.
  wm::ActivateWindow(non_ash_window.get());
  EXPECT_FALSE(input_method().focused());
}

// Tests that IME stays un-focused when the active window changes between
// different ClientWindows.
TEST_F(ImeFocusHandlerTest, BetweenClientWindows) {
  // This test relies on state only set in classic.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  // Activates a non-ash window. IME should lose focus.
  std::unique_ptr<aura::Window> non_ash_window_1 = CreateRemoteWindow();
  wm::ActivateWindow(non_ash_window_1.get());
  EXPECT_FALSE(input_method().focused());

  // Activates another non-ash window. IME should not be focused.
  std::unique_ptr<aura::Window> non_ash_window_2 = CreateRemoteWindow();
  wm::ActivateWindow(non_ash_window_2.get());
  EXPECT_FALSE(input_method().focused());
}

// Tests that IME stays focused when the active window changes between ash
// windows.
TEST_F(ImeFocusHandlerTest, BetweenAshWindows) {
  // This test relies on state only set in classic.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  // Activates an ash window. IME is focused.
  std::unique_ptr<aura::Window> ash_window_1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 50)));
  wm::ActivateWindow(ash_window_1.get());
  EXPECT_TRUE(input_method().focused());

  // Activates another ash window. IME is still focused.
  std::unique_ptr<aura::Window> ash_window_2(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 50)));
  wm::ActivateWindow(ash_window_2.get());
  EXPECT_TRUE(input_method().focused());
}

}  // namespace ash
