// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_quit_bubble_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ui/views/confirm_quit_bubble.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/slide_animation.h"

class TestConfirmQuitBubble : public ConfirmQuitBubbleBase {
 public:
  TestConfirmQuitBubble() {}
  ~TestConfirmQuitBubble() override {}

  void Show() override {}
  void Hide() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestConfirmQuitBubble);
};

class TestConfirmQuitBubbleController : public ConfirmQuitBubbleController {
 public:
  TestConfirmQuitBubbleController(
      std::unique_ptr<ConfirmQuitBubbleBase> bubble,
      std::unique_ptr<base::Timer> hide_timer,
      std::unique_ptr<gfx::SlideAnimation> animation)
      : ConfirmQuitBubbleController(std::move(bubble),
                                    std::move(hide_timer),
                                    std::move(animation)) {}

  void DeactivateBrowser() { OnBrowserNoLongerActive(nullptr); }

  bool quit_called() const { return quit_called_; }

 private:
  void DoQuit() override { quit_called_ = true; }

  bool IsFeatureEnabled() override { return true; }

  bool quit_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestConfirmQuitBubbleController);
};

class TestSlideAnimation : public gfx::SlideAnimation {
 public:
  TestSlideAnimation() : gfx::SlideAnimation(nullptr) {}
  ~TestSlideAnimation() override {}

  void Reset() override {}
  void Reset(double value) override {}
  void Show() override {}
  void Hide() override {}
  void SetSlideDuration(int duration) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSlideAnimation);
};

class ConfirmQuitBubbleControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<TestConfirmQuitBubble> bubble =
        std::make_unique<TestConfirmQuitBubble>();
    std::unique_ptr<base::MockTimer> timer =
        std::make_unique<base::MockTimer>(false, false);
    bubble_ = bubble.get();
    timer_ = timer.get();
    controller_.reset(new TestConfirmQuitBubbleController(
        std::move(bubble), std::move(timer),
        std::make_unique<TestSlideAnimation>()));
  }

  void TearDown() override { controller_.reset(); }

  void SendAccelerator(bool quit, bool press, bool repeat) {
    ui::KeyboardCode key = quit ? ui::VKEY_Q : ui::VKEY_P;
    int modifiers = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN;
    if (repeat)
      modifiers |= ui::EF_IS_REPEAT;
    ui::EventType type = press ? ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED;
    ui::KeyEvent event(type, key, modifiers);
    controller_->OnKeyEvent(&event);
  }

  void PressQuitAccelerator() { SendAccelerator(true, true, false); }

  void ReleaseQuitAccelerator() { SendAccelerator(true, false, false); }

  void RepeatQuitAccelerator() { SendAccelerator(true, true, true); }

  void PressOtherAccelerator() { SendAccelerator(false, true, false); }

  void ReleaseOtherAccelerator() { SendAccelerator(false, false, false); }

  std::unique_ptr<TestConfirmQuitBubbleController> controller_;

  // Owned by |controller_|.
  TestConfirmQuitBubble* bubble_;

  // Owned by |controller_|.
  base::MockTimer* timer_;
};

// Pressing and holding the shortcut should quit.
TEST_F(ConfirmQuitBubbleControllerTest, PressAndHold) {
  PressQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  timer_->Fire();
  EXPECT_FALSE(controller_->quit_called());
  ReleaseQuitAccelerator();
  EXPECT_TRUE(controller_->quit_called());
}

// Pressing the shortcut twice should quit.
TEST_F(ConfirmQuitBubbleControllerTest, DoublePress) {
  PressQuitAccelerator();
  ReleaseQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  PressQuitAccelerator();
  EXPECT_FALSE(timer_->IsRunning());
  EXPECT_FALSE(controller_->quit_called());
  ReleaseQuitAccelerator();
  EXPECT_TRUE(controller_->quit_called());
}

// Pressing the shortcut once should not quit.
TEST_F(ConfirmQuitBubbleControllerTest, SinglePress) {
  PressQuitAccelerator();
  ReleaseQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  timer_->Fire();
  EXPECT_FALSE(controller_->quit_called());
}

// Repeated presses should not be counted.
TEST_F(ConfirmQuitBubbleControllerTest, RepeatedPresses) {
  PressQuitAccelerator();
  RepeatQuitAccelerator();
  ReleaseQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  timer_->Fire();
  EXPECT_FALSE(controller_->quit_called());
}

// Other keys shouldn't matter.
TEST_F(ConfirmQuitBubbleControllerTest, OtherKeyPress) {
  PressQuitAccelerator();
  ReleaseQuitAccelerator();
  PressOtherAccelerator();
  ReleaseOtherAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  PressQuitAccelerator();
  EXPECT_FALSE(timer_->IsRunning());
  EXPECT_FALSE(controller_->quit_called());
  ReleaseQuitAccelerator();
  EXPECT_TRUE(controller_->quit_called());
}

// The controller state should be reset when the browser loses focus.
TEST_F(ConfirmQuitBubbleControllerTest, BrowserLosesFocus) {
  // Press but don't release the accelerator.
  PressQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  controller_->DeactivateBrowser();
  EXPECT_FALSE(timer_->IsRunning());
  EXPECT_FALSE(controller_->quit_called());
  ReleaseQuitAccelerator();

  // Press and release the accelerator.
  PressQuitAccelerator();
  ReleaseQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  controller_->DeactivateBrowser();
  EXPECT_FALSE(timer_->IsRunning());
  EXPECT_FALSE(controller_->quit_called());

  // Press and hold the accelerator.
  PressQuitAccelerator();
  EXPECT_TRUE(timer_->IsRunning());
  timer_->Fire();
  EXPECT_FALSE(timer_->IsRunning());
  controller_->DeactivateBrowser();
  ReleaseQuitAccelerator();
  EXPECT_FALSE(controller_->quit_called());
}
