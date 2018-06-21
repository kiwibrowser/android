// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "services/ui/ws2/test_window_tree_client.h"
#include "services/ui/ws2/window_tree_test_helper.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace ash {

// This test creates a top-level window via the WindowService in SetUp() and
// provides some ease of use functions to access WindowService related state.
class WindowServiceDelegateImplTest : public AshTestBase {
 public:
  WindowServiceDelegateImplTest() = default;
  ~WindowServiceDelegateImplTest() override = default;

  ui::Id GetTopLevelWindowId() {
    return GetWindowTreeTestHelper()->TransportIdForWindow(top_level_.get());
  }

  wm::WmToplevelWindowEventHandler* event_handler() {
    return Shell::Get()
        ->toplevel_window_event_handler()
        ->wm_toplevel_window_event_handler();
  }

  std::vector<ui::ws2::Change>* GetWindowTreeClientChanges() {
    return GetTestWindowTreeClient()->tracker()->changes();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    top_level_ = CreateTestWindow(gfx::Rect(100, 100, 100, 100));
    ASSERT_TRUE(top_level_);
    GetEventGenerator().PressLeftButton();
  }
  void TearDown() override {
    // Ash owns the WindowTree, which also handles deleting |top_level_|. This
    // needs to delete |top_level_| before the WindowTree is deleted, otherwise
    // the WindowTree will delete |top_level_|, leading to a double delete.
    top_level_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<aura::Window> top_level_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowServiceDelegateImplTest);
};

TEST_F(WindowServiceDelegateImplTest, RunWindowMoveLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ui::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator().MoveMouseTo(gfx::Point(5, 6));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
  GetWindowTreeClientChanges()->clear();
  GetEventGenerator().ReleaseLeftButton();

  // Releasing the mouse completes the move loop.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=true"));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
}

TEST_F(WindowServiceDelegateImplTest, DeleteWindowWithInProgressRunLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      29, GetTopLevelWindowId(), ui::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  top_level_.reset();
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  // Deleting the window implicitly cancels the drag.
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=29 success=false"));
}

TEST_F(WindowServiceDelegateImplTest, CancelWindowMoveLoop) {
  GetWindowTreeTestHelper()->window_tree()->PerformWindowMove(
      21, GetTopLevelWindowId(), ui::mojom::MoveLoopSource::MOUSE,
      gfx::Point());
  EXPECT_TRUE(event_handler()->is_drag_in_progress());
  GetEventGenerator().MoveMouseTo(gfx::Point(5, 6));
  EXPECT_EQ(gfx::Point(105, 106), top_level_->bounds().origin());
  GetWindowTreeClientChanges()->clear();
  GetWindowTreeTestHelper()->window_tree()->CancelWindowMove(
      GetTopLevelWindowId());
  EXPECT_FALSE(event_handler()->is_drag_in_progress());
  EXPECT_TRUE(ContainsChange(*GetWindowTreeClientChanges(),
                             "ChangeCompleted id=21 success=false"));
  EXPECT_EQ(gfx::Point(100, 100), top_level_->bounds().origin());
}

}  // namespace ash
