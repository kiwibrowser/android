// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#import "testing/gtest_mac.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/event_utils.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {
namespace {

constexpr int kTestCommandId = 0;

class TestModel : public ui::SimpleMenuModel {
 public:
  TestModel() : ui::SimpleMenuModel(&delegate_), delegate_(this) {}

  void set_checked_command(int command) { checked_command_ = command; }

  void set_menu_open_callback(base::OnceClosure callback) {
    menu_open_callback_ = std::move(callback);
  }

 private:
  class Delegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit Delegate(TestModel* model) : model_(model) {}
    bool IsCommandIdChecked(int command_id) const override {
      return command_id == model_->checked_command_;
    }
    bool IsCommandIdEnabled(int command_id) const override { return true; }
    void ExecuteCommand(int command_id, int event_flags) override {}

    void MenuWillShow(SimpleMenuModel* source) override {
      if (!model_->menu_open_callback_.is_null())
        std::move(model_->menu_open_callback_).Run();
    }

    bool GetAcceleratorForCommandId(
        int command_id,
        ui::Accelerator* accelerator) const override {
      if (command_id == kTestCommandId) {
        *accelerator = ui::Accelerator(ui::VKEY_E, ui::EF_CONTROL_DOWN);
        return true;
      }
      return false;
    }

   private:
    TestModel* model_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

 private:
  int checked_command_ = -1;
  Delegate delegate_;
  base::OnceClosure menu_open_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestModel);
};

}  // namespace

class MenuRunnerCocoaTest : public ViewsTestBase {
 public:
  enum {
    kWindowHeight = 200,
    kWindowOffset = 100,
  };

  MenuRunnerCocoaTest() {}
  ~MenuRunnerCocoaTest() override {}

  void SetUp() override {
    const int kWindowWidth = 300;
    ViewsTestBase::SetUp();

    menu_.reset(new TestModel());
    menu_->AddCheckItem(kTestCommandId, base::ASCIIToUTF16("Menu Item"));

    parent_ = new views::Widget();
    parent_->Init(CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    parent_->SetBounds(
        gfx::Rect(kWindowOffset, kWindowOffset, kWindowWidth, kWindowHeight));
    parent_->Show();

    base::Closure on_close = base::Bind(&MenuRunnerCocoaTest::MenuCloseCallback,
                                        base::Unretained(this));
    runner_ = new internal::MenuRunnerImplAdapter(menu_.get(), on_close);
    EXPECT_FALSE(runner_->IsRunning());
  }

  void TearDown() override {
    if (runner_) {
      runner_->Release();
      runner_ = NULL;
    }

    parent_->CloseNow();
    ViewsTestBase::TearDown();
  }

  // Runs the menu after registering |callback| as the menu open callback.
  void RunMenu(base::OnceClosure callback) {
    // Cancelling an async menu under MenuControllerCocoa::OpenMenuImpl()
    // (which invokes WillShowMenu()) will cause a UAF when that same function
    // tries to show the menu. So post a task instead.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));

    runner_->RunMenuAt(parent_, nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                       MenuRunner::CONTEXT_MENU);
    MaybeRunAsync();
  }

  // Runs then cancels a combobox menu and captures the frame of the anchoring
  // view.
  void RunMenuAt(const gfx::Rect& anchor) {
    // Should be one child (the compositor layer) before showing, and it should
    // go up by one (the anchor view) while the menu is shown.
    EXPECT_EQ(1u, [[parent_->GetNativeView() subviews] count]);

    base::OnceClosure callback =
        base::BindOnce(&MenuRunnerCocoaTest::ComboboxRunMenuAtCallback,
                       base::Unretained(this));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));

    runner_->RunMenuAt(parent_, nullptr, anchor, MENU_ANCHOR_TOPLEFT,
                       MenuRunner::COMBOBOX);
    MaybeRunAsync();

    // Ensure the anchor view is removed.
    EXPECT_EQ(1u, [[parent_->GetNativeView() subviews] count]);
  }

  void MenuCancelCallback() {
    runner_->Cancel();
    // Async menus report their cancellation immediately.
    EXPECT_FALSE(runner_->IsRunning());
  }

  void MenuDeleteCallback() {
    runner_->Release();
    runner_ = nullptr;
    // Deleting an async menu intentionally does not invoke MenuCloseCallback().
    // (The callback is typically a method on something in the process of being
    // destroyed). So invoke QuitAsyncRunLoop() here as well.
    QuitAsyncRunLoop();
  }

  void ModelDeleteThenSelectItemCallback() {
    // A View showing a menu typically owns a MenuRunner unique_ptr, which will
    // will be destroyed (releasing the MenuRunnerImpl) alongside the MenuModel.
    runner_->Release();
    runner_ = nullptr;
    menu_ = nullptr;

    // The menu is closing (yet "alive"), but the model is destroyed. The user
    // may have already made an event to select an item in the menu. This
    // doesn't bother views menus (see MenuRunnerImpl::empty_delegate_) but
    // Cocoa menu items are refcounted and have access to a raw weak pointer in
    // the MenuController.
    QuitAsyncRunLoop();
  }

  void MenuCancelAndDeleteCallback() {
    runner_->Cancel();
    runner_->Release();
    runner_ = nullptr;
  }

 protected:
  std::unique_ptr<TestModel> menu_;
  internal::MenuRunnerImplInterface* runner_ = nullptr;
  views::Widget* parent_ = nullptr;
  int menu_close_count_ = 0;

 private:
  void RunMenuWrapperCallback(base::OnceClosure callback) {
    EXPECT_TRUE(runner_->IsRunning());
    std::move(callback).Run();
  }

  void ComboboxRunMenuAtCallback() {
    NSArray* subviews = [parent_->GetNativeView() subviews];
    // An anchor view should only be added for Native menus.
    EXPECT_EQ(1u, [subviews count]);
    runner_->Cancel();
  }

  // Run a nested run loop so that async and sync menus can be tested the
  // same way.
  void MaybeRunAsync() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();

    // |quit_closure_| should be run by QuitAsyncRunLoop(), not the timeout.
    EXPECT_TRUE(quit_closure_.is_null());
  }

  void QuitAsyncRunLoop() {
    ASSERT_FALSE(quit_closure_.is_null());
    quit_closure_.Run();
    quit_closure_.Reset();
  }

  void MenuCloseCallback() {
    ++menu_close_count_;
    QuitAsyncRunLoop();
  }

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerCocoaTest);
};

TEST_F(MenuRunnerCocoaTest, RunMenuAndCancel) {
  RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuCancelCallback,
                         base::Unretained(this)));

  EXPECT_EQ(1, menu_close_count_);
  EXPECT_FALSE(runner_->IsRunning());

  // MenuItemView's MenuRunnerImpl gets the closing time from
  // MenuControllerCocoa:: closing_event_time(). This is is reset on show, but
  // only updated when an event closes the menu -- not a cancellation.
  EXPECT_EQ(runner_->GetClosingEventTime(), base::TimeTicks());
  EXPECT_LE(runner_->GetClosingEventTime(), ui::EventTimeForNow());

  // Cancel again.
  runner_->Cancel();
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_EQ(1, menu_close_count_);
}

TEST_F(MenuRunnerCocoaTest, RunMenuAndDelete) {
  RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuDeleteCallback,
                         base::Unretained(this)));
  // Note the close callback is NOT invoked for deleted menus.
  EXPECT_EQ(0, menu_close_count_);
}

// Tests a potential lifetime issue using the Cocoa MenuController, which has a
// weak reference to the model.
TEST_F(MenuRunnerCocoaTest, RunMenuAndDeleteThenSelectItem) {
  RunMenu(
      base::BindOnce(&MenuRunnerCocoaTest::ModelDeleteThenSelectItemCallback,
                     base::Unretained(this)));
  EXPECT_EQ(0, menu_close_count_);
}

// Ensure a menu can be safely released immediately after a call to Cancel() in
// the same run loop iteration.
TEST_F(MenuRunnerCocoaTest, DestroyAfterCanceling) {
  RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuCancelAndDeleteCallback,
                         base::Unretained(this)));

  EXPECT_EQ(1, menu_close_count_);
}

TEST_F(MenuRunnerCocoaTest, RunMenuTwice) {
  for (int i = 0; i < 2; ++i) {
    RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuCancelCallback,
                           base::Unretained(this)));
    EXPECT_FALSE(runner_->IsRunning());
    EXPECT_EQ(i + 1, menu_close_count_);
  }
}

TEST_F(MenuRunnerCocoaTest, CancelWithoutRunning) {
  runner_->Cancel();
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_EQ(base::TimeTicks(), runner_->GetClosingEventTime());
  EXPECT_EQ(0, menu_close_count_);
}

TEST_F(MenuRunnerCocoaTest, DeleteWithoutRunning) {
  runner_->Release();
  runner_ = NULL;
  EXPECT_EQ(0, menu_close_count_);
}

}  // namespace test
}  // namespace views
