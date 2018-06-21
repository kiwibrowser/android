// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws2/window_tree.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <queue>

#include "base/run_loop.h"
#include "base/unguessable_token.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/ws2/event_test_utils.h"
#include "services/ui/ws2/server_window.h"
#include "services/ui/ws2/server_window_test_helper.h"
#include "services/ui/ws2/window_service.h"
#include "services/ui/ws2/window_service_test_setup.h"
#include "services/ui/ws2/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/focus_controller.h"

namespace ui {
namespace ws2 {
namespace {

// Passed to Embed() to give the default behavior (see kEmbedFlag* in mojom for
// details).
constexpr uint32_t kDefaultEmbedFlags = 0;

class TestLayoutManager : public aura::LayoutManager {
 public:
  TestLayoutManager() = default;
  ~TestLayoutManager() override = default;

  void set_next_bounds(const gfx::Rect& bounds) { next_bounds_ = bounds; }

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    if (next_bounds_) {
      SetChildBoundsDirect(child, *next_bounds_);
      next_bounds_.reset();
    } else {
      SetChildBoundsDirect(child, requested_bounds);
    }
  }

 private:
  base::Optional<gfx::Rect> next_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutManager);
};

// Used as callback from ScheduleEmbed().
void ScheduleEmbedCallback(base::UnguessableToken* result_token,
                           const base::UnguessableToken& actual_token) {
  *result_token = actual_token;
}

// Used as callback to EmbedUsingToken().
void EmbedUsingTokenCallback(bool* was_called,
                             bool* result_value,
                             bool actual_result) {
  *was_called = true;
  *result_value = actual_result;
}

TEST(WindowTreeTest2, NewWindow) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest2, NewWindowWithProperties) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* window = setup.window_tree_test_helper()->NewWindow(
      1, {{ui::mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(window);
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(window->GetProperty(aura::client::kAlwaysOnTopKey));
}

TEST(WindowTreeTest2, NewTopLevelWindow) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest2, NewTopLevelWindowWithProperties) {
  WindowServiceTestSetup setup;
  EXPECT_TRUE(setup.changes()->empty());
  aura::PropertyConverter::PrimitiveType value = true;
  std::vector<uint8_t> transport = mojo::ConvertTo<std::vector<uint8_t>>(value);
  aura::Window* top_level = setup.window_tree_test_helper()->NewTopLevelWindow(
      1, {{ui::mojom::WindowManager::kAlwaysOnTop_Property, transport}});
  ASSERT_TRUE(top_level);
  EXPECT_EQ("TopLevelCreated id=1 window_id=0,1 drawn=false",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
}

TEST(WindowTreeTest2, SetTopLevelWindowBounds) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();

  const gfx::Rect bounds_from_client = gfx::Rect(1, 2, 300, 400);
  setup.window_tree_test_helper()->SetWindowBoundsWithAck(
      top_level, bounds_from_client, 2);
  EXPECT_EQ(bounds_from_client, top_level->bounds());
  ASSERT_EQ(2u, setup.changes()->size());
  {
    const Change& change = (*setup.changes())[0];
    EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, change.type);
    EXPECT_EQ(top_level->bounds(), change.bounds2);
    EXPECT_TRUE(change.local_surface_id);
    setup.changes()->erase(setup.changes()->begin());
  }
  // See comments in WindowTree::SetBoundsImpl() for why this returns false.
  EXPECT_EQ("ChangeCompleted id=2 success=false",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  const gfx::Rect bounds_from_server = gfx::Rect(101, 102, 103, 104);
  top_level->SetBounds(bounds_from_server);
  ASSERT_EQ(1u, setup.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*setup.changes())[0].type);
  EXPECT_EQ(bounds_from_server, (*setup.changes())[0].bounds2);
  setup.changes()->clear();

  // Set a LayoutManager so that when the client requests a bounds change the
  // window is resized to a different bounds.
  // |layout_manager| is owned by top_level->parent();
  TestLayoutManager* layout_manager = new TestLayoutManager();
  const gfx::Rect restricted_bounds = gfx::Rect(401, 405, 406, 407);
  layout_manager->set_next_bounds(restricted_bounds);
  top_level->parent()->SetLayoutManager(layout_manager);
  setup.window_tree_test_helper()->SetWindowBoundsWithAck(
      top_level, bounds_from_client, 3);
  ASSERT_EQ(2u, setup.changes()->size());
  // The layout manager changes the bounds to a different value than the client
  // requested, so the client should get OnWindowBoundsChanged() with
  // |restricted_bounds|.
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, (*setup.changes())[0].type);
  EXPECT_EQ(restricted_bounds, (*setup.changes())[0].bounds2);

  // And because the layout manager changed the bounds the result is false.
  EXPECT_EQ("ChangeCompleted id=3 success=false",
            ChangeToDescription((*setup.changes())[1]));
}

TEST(WindowTreeTest2, SetTopLevelWindowBoundsFailsForSameSize) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();
  const gfx::Rect bounds = gfx::Rect(1, 2, 300, 400);
  top_level->SetBounds(bounds);
  setup.changes()->clear();
  // WindowTreeTest2Helper::SetWindowBounds() uses a null LocalSurfaceId, which
  // differs from the current LocalSurfaceId (assigned by ClientRoot). Because
  // of this, the LocalSurfaceIds differ and the call returns false.
  EXPECT_FALSE(
      setup.window_tree_test_helper()->SetWindowBounds(top_level, bounds));
  EXPECT_TRUE(setup.changes()->empty());
}

TEST(WindowTreeTest2, SetChildWindowBounds) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  const gfx::Rect bounds = gfx::Rect(1, 2, 300, 400);
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(window, bounds));
  EXPECT_EQ(bounds, window->bounds());

  // Setting to same bounds should return true.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(window, bounds));
  EXPECT_EQ(bounds, window->bounds());
}

TEST(WindowTreeTest2, SetBoundsAtEmbedWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  const gfx::Rect bounds1 = gfx::Rect(1, 2, 300, 400);
  EXPECT_TRUE(
      setup.window_tree_test_helper()->SetWindowBounds(window, bounds1));

  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);

  // Child client should not be able to change bounds of embed window.
  EXPECT_FALSE(embedding_helper->window_tree_test_helper->SetWindowBounds(
      window, gfx::Rect()));
  // Bounds should not have changed.
  EXPECT_EQ(bounds1, window->bounds());

  embedding_helper->window_tree_client.tracker()->changes()->clear();
  embedding_helper->window_tree_client.set_track_root_bounds_changes(true);

  // Set the bounds from the parent and ensure client is notified.
  const gfx::Rect bounds2 = gfx::Rect(1, 2, 300, 401);
  base::Optional<viz::LocalSurfaceId> local_surface_id(
      viz::LocalSurfaceId(1, 2, base::UnguessableToken::Create()));
  EXPECT_TRUE(setup.window_tree_test_helper()->SetWindowBounds(
      window, bounds2, local_surface_id));
  EXPECT_EQ(bounds2, window->bounds());
  ASSERT_EQ(1u,
            embedding_helper->window_tree_client.tracker()->changes()->size());
  const Change bounds_change =
      (*(embedding_helper->window_tree_client.tracker()->changes()))[0];
  EXPECT_EQ(CHANGE_TYPE_NODE_BOUNDS_CHANGED, bounds_change.type);
  EXPECT_EQ(bounds2, bounds_change.bounds2);
  EXPECT_EQ(local_surface_id, bounds_change.local_surface_id);
}

// Tests the ability of the client to change properties on the server.
TEST(WindowTreeTest2, SetTopLevelWindowProperty) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  setup.changes()->clear();

  EXPECT_FALSE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  aura::PropertyConverter::PrimitiveType client_value = true;
  std::vector<uint8_t> client_transport_value =
      mojo::ConvertTo<std::vector<uint8_t>>(client_value);
  setup.window_tree_test_helper()->SetWindowProperty(
      top_level, ui::mojom::WindowManager::kAlwaysOnTop_Property,
      client_transport_value, 2);
  EXPECT_EQ("ChangeCompleted id=2 success=true",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
  setup.changes()->clear();

  top_level->SetProperty(aura::client::kAlwaysOnTopKey, false);
  EXPECT_EQ(
      "PropertyChanged window=0,1 key=prop:always_on_top "
      "value=0000000000000000",
      SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest2, WindowToWindowData) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  setup.changes()->clear();

  window->SetBounds(gfx::Rect(1, 2, 300, 400));
  window->SetProperty(aura::client::kAlwaysOnTopKey, true);
  window->Show();  // Called to make the window visible.
  mojom::WindowDataPtr data =
      setup.window_tree_test_helper()->WindowToWindowData(window);
  EXPECT_EQ(gfx::Rect(1, 2, 300, 400), data->bounds);
  EXPECT_TRUE(data->visible);
  EXPECT_EQ(1u, data->properties.count(
                    ui::mojom::WindowManager::kAlwaysOnTop_Property));
  EXPECT_EQ(
      aura::PropertyConverter::PrimitiveType(true),
      mojo::ConvertTo<aura::PropertyConverter::PrimitiveType>(
          data->properties[ui::mojom::WindowManager::kAlwaysOnTop_Property]));
}

TEST(WindowTreeTest2, MovePressDragRelease) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  EXPECT_EQ("POINTER_MOVED 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.PressLeftButton();
  EXPECT_EQ("POINTER_DOWN 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.MoveMouseTo(0, 0);
  EXPECT_EQ("POINTER_MOVED -10,-10",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.ReleaseLeftButton();
  EXPECT_EQ("POINTER_UP -10,-10",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
}

// Used to verify destruction with a touch pointer down doesn't crash.
TEST(WindowTreeTest2, ShutdownWithTouchDown) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  test::EventGenerator event_generator(setup.root());
  event_generator.set_current_location(gfx::Point(50, 51));
  event_generator.PressTouch();
}

TEST(WindowTreeTest2, TouchPressDragRelease) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 11, 100, 100));

  test::EventGenerator event_generator(setup.root());
  event_generator.set_current_location(gfx::Point(50, 51));
  event_generator.PressTouch();
  EXPECT_EQ("POINTER_DOWN 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.MoveTouch(gfx::Point(5, 6));
  EXPECT_EQ("POINTER_MOVED -5,-5",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  event_generator.ReleaseTouch();
  EXPECT_EQ("POINTER_UP -5,-5",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
}

class EventRecordingWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  EventRecordingWindowDelegate() = default;
  ~EventRecordingWindowDelegate() override = default;

  std::queue<std::unique_ptr<ui::Event>>& events() { return events_; }

  std::unique_ptr<Event> PopEvent() {
    if (events_.empty())
      return nullptr;
    auto event = std::move(events_.front());
    events_.pop();
    return event;
  }

  void ClearEvents() {
    std::queue<std::unique_ptr<ui::Event>> events;
    std::swap(events_, events);
  }

  // aura::test::TestWindowDelegate:
  void OnEvent(ui::Event* event) override {
    events_.push(ui::Event::Clone(*event));
  }

 private:
  std::queue<std::unique_ptr<ui::Event>> events_;

  DISALLOW_COPY_AND_ASSIGN(EventRecordingWindowDelegate);
};

TEST(WindowTreeTest2, MoveFromClientToNonClient) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  window_delegate.ClearEvents();

  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  EXPECT_EQ("POINTER_MOVED 40,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  // The delegate should see the same events (but as mouse events).
  EXPECT_EQ("MOUSE_ENTERED 40,40", LocatedEventToEventTypeAndLocation(
                                       window_delegate.PopEvent().get()));
  EXPECT_EQ("MOUSE_MOVED 40,40", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));

  // Move the mouse over the non-client area.
  // The event is still sent to the client, and the delegate.
  event_generator.MoveMouseTo(15, 16);
  EXPECT_EQ("POINTER_MOVED 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  // Delegate should also get the events.
  EXPECT_EQ("MOUSE_MOVED 5,6", LocatedEventToEventTypeAndLocation(
                                   window_delegate.PopEvent().get()));

  // Only the delegate should get the press in this case.
  event_generator.PressLeftButton();
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());

  EXPECT_EQ("MOUSE_PRESSED 5,6", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));

  // Move mouse into client area, only the delegate should get the move (drag).
  event_generator.MoveMouseTo(35, 51);
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());

  EXPECT_EQ("MOUSE_DRAGGED 25,41", LocatedEventToEventTypeAndLocation(
                                       window_delegate.PopEvent().get()));

  // Release over client area, again only delegate should get it.
  event_generator.ReleaseLeftButton();
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());

  EXPECT_EQ("MOUSE_RELEASED",
            EventToEventType(window_delegate.PopEvent().get()));

  event_generator.MoveMouseTo(26, 50);
  EXPECT_EQ("POINTER_MOVED 16,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  // Delegate should also get the events.
  EXPECT_EQ("MOUSE_MOVED 16,40", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));

  // Press in client area. Only the client should get the event.
  event_generator.PressLeftButton();
  EXPECT_EQ("POINTER_DOWN 16,40",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));

  ASSERT_FALSE(window_delegate.PopEvent().get());
}

TEST(WindowTreeTest2, MouseDownInNonClientWithChildWindow) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  // Add a child Window that is sized to fill the top-level.
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  window->Show();
  window->SetBounds(gfx::Rect(top_level->bounds().size()));
  top_level->AddChild(window);

  window_delegate.ClearEvents();

  // Move the mouse over the non-client area. Both the client and the delegate
  // should get the event.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(15, 16);
  EXPECT_EQ("POINTER_MOVED 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopInputEvent().event.get()));
  EXPECT_TRUE(window_tree_client->input_events().empty());
  EXPECT_EQ("MOUSE_ENTERED",
            EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_EQ("MOUSE_MOVED", EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_TRUE(window_delegate.events().empty());

  // Press over the non-client. The client should not be notified as the event
  // should be handled locally.
  event_generator.PressLeftButton();
  ASSERT_FALSE(window_tree_client->PopInputEvent().event.get());
  EXPECT_EQ("MOUSE_PRESSED 5,6", LocatedEventToEventTypeAndLocation(
                                     window_delegate.PopEvent().get()));
}

TEST(WindowTreeTest2, MouseDownInNonClientDragToClientWithChildWindow) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  // Add a child Window that is sized to fill the top-level.
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  window->Show();
  window->SetBounds(gfx::Rect(top_level->bounds().size()));
  top_level->AddChild(window);

  // Press in non-client area.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(15, 16);
  event_generator.PressLeftButton();

  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  window_tree_client->ClearInputEvents();
  window_delegate.ClearEvents();
  // Drag over client area, only the delegate should get it (because the press
  // was in the non-client area).
  event_generator.MoveMouseTo(15, 26);
  EXPECT_EQ("MOUSE_DRAGGED",
            EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_TRUE(window_tree_client->input_events().empty());
}

TEST(WindowTreeTest2, PointerWatcher) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  setup.window_tree_test_helper()->SetEventTargetingPolicy(
      top_level, mojom::EventTargetingPolicy::NONE);
  EXPECT_EQ(mojom::EventTargetingPolicy::NONE,
            top_level->event_targeting_policy());
  // Start the pointer watcher only for pointer down/up.
  setup.window_tree_test_helper()->window_tree()->StartPointerWatcher(false);

  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));

  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  ASSERT_TRUE(window_tree_client->observed_pointer_events().empty());

  event_generator.MoveMouseTo(5, 6);
  ASSERT_TRUE(window_tree_client->observed_pointer_events().empty());

  event_generator.PressLeftButton();
  EXPECT_EQ("POINTER_DOWN 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedPointerEvent().event.get()));

  event_generator.ReleaseLeftButton();
  EXPECT_EQ("POINTER_UP 5,6",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedPointerEvent().event.get()));

  // Enable observing move events.
  setup.window_tree_test_helper()->window_tree()->StartPointerWatcher(true);
  event_generator.MoveMouseTo(8, 9);
  EXPECT_EQ("POINTER_MOVED 8,9",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedPointerEvent().event.get()));

  const int kTouchId = 11;
  event_generator.MoveTouchId(gfx::Point(2, 3), kTouchId);
  EXPECT_EQ("POINTER_MOVED 2,3",
            LocatedEventToEventTypeAndLocation(
                window_tree_client->PopObservedPointerEvent().event.get()));
}

TEST(WindowTreeTest2, MatchesPointerWatcherSet) {
  WindowServiceTestSetup setup;
  TestWindowTreeClient* window_tree_client = setup.window_tree_client();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow(1);
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(10, 10, 100, 100));
  // Start the pointer watcher only for pointer down/up.
  setup.window_tree_test_helper()->window_tree()->StartPointerWatcher(false);

  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(50, 50);
  EXPECT_TRUE(window_tree_client->observed_pointer_events().empty());
  window_tree_client->ClearInputEvents();

  event_generator.PressLeftButton();
  // The client should get the event, and |matches_pointer_watcher| should be
  // true (because it matched the pointer watcher).
  TestWindowTreeClient::InputEvent press_input =
      window_tree_client->PopInputEvent();
  ASSERT_TRUE(press_input.event);
  EXPECT_EQ("POINTER_DOWN 40,40",
            LocatedEventToEventTypeAndLocation(press_input.event.get()));
  EXPECT_TRUE(press_input.matches_pointer_watcher);
  // Because the event matches a pointer event there should be no observed
  // pointer events.
  EXPECT_TRUE(window_tree_client->observed_pointer_events().empty());
}

TEST(WindowTreeTest2, Capture) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();

  // Setting capture on |window| should fail as it's not visible.
  EXPECT_FALSE(setup.window_tree_test_helper()->SetCapture(window));

  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  EXPECT_FALSE(setup.window_tree_test_helper()->SetCapture(top_level));
  top_level->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));

  EXPECT_FALSE(setup.window_tree_test_helper()->ReleaseCapture(window));
  EXPECT_TRUE(setup.window_tree_test_helper()->ReleaseCapture(top_level));

  top_level->AddChild(window);
  window->Show();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.window_tree_test_helper()->ReleaseCapture(window));
}

TEST(WindowTreeTest2, TransferCaptureToClient) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));

  wm::CaptureController::Get()->SetCapture(top_level);
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(6, 6);
  setup.window_tree_client()->ClearInputEvents();
  window_delegate.ClearEvents();
  event_generator.MoveMouseTo(7, 7);

  // Because capture was initiated locally event should go to |window_delegate|
  // only (not the client).
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_EQ("MOUSE_MOVED", EventToEventType(window_delegate.PopEvent().get()));
  EXPECT_TRUE(window_delegate.events().empty());

  // Request capture from the client.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  event_generator.MoveMouseTo(8, 8);
  // Now the event should go to the client and not local.
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_EQ("POINTER_MOVED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
}

TEST(WindowTreeTest2, TransferCaptureBetweenParentAndChild) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  top_level->AddChild(window);
  window->Show();
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);

  // Move the mouse and set capture from the child.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(6, 6);
  setup.window_tree_client()->ClearInputEvents();
  window_delegate.ClearEvents();
  embedding_helper->window_tree_client.ClearInputEvents();
  EXPECT_TRUE(embedding_helper->window_tree_test_helper->SetCapture(window));
  event_generator.MoveMouseTo(7, 7);

  // As capture was set from the child, only the child should get the event.
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_EQ(
      "POINTER_MOVED",
      EventToEventType(
          embedding_helper->window_tree_client.PopInputEvent().event.get()));
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());

  // Set capture from the parent, only the parent should get the event now.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  event_generator.MoveMouseTo(8, 8);
  EXPECT_EQ("POINTER_MOVED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());
}

TEST(WindowTreeTest2, CaptureNotification) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  setup.changes()->clear();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.changes()->empty());

  wm::CaptureController::Get()->ReleaseCapture(window);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(setup.changes())));
}

TEST(WindowTreeTest2, CaptureNotificationForEmbedRoot) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  setup.changes()->clear();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.changes()->empty());

  // Set capture on the embed-root from the embedded client. The embedder
  // should be notified.
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window);
  ASSERT_TRUE(embedding_helper);
  setup.changes()->clear();
  embedding_helper->changes()->clear();
  EXPECT_TRUE(embedding_helper->window_tree_test_helper->SetCapture(window));
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(setup.changes())));
  setup.changes()->clear();
  EXPECT_TRUE(embedding_helper->changes()->empty());

  // Set capture from the embedder. This triggers the embedded client to lose
  // capture.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.changes()->empty());
  // NOTE: the '2' is because the embedded client sees the high order bits of
  // the root.
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=2,1",
            SingleChangeToDescription(*(embedding_helper->changes())));
  embedding_helper->changes()->clear();

  // And release capture locally.
  wm::CaptureController::Get()->ReleaseCapture(window);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,1",
            SingleChangeToDescription(*(setup.changes())));
  EXPECT_TRUE(embedding_helper->changes()->empty());
}

TEST(WindowTreeTest2, CaptureNotificationForTopLevel) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow(11);
  ASSERT_TRUE(top_level);
  top_level->Show();
  setup.changes()->clear();
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  EXPECT_TRUE(setup.changes()->empty());

  // Release capture locally.
  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  capture_controller->ReleaseCapture(top_level);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,11",
            SingleChangeToDescription(*(setup.changes())));
  setup.changes()->clear();

  // Set capture locally.
  capture_controller->SetCapture(top_level);
  EXPECT_TRUE(setup.changes()->empty());

  // Set capture from client.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(top_level));
  EXPECT_TRUE(setup.changes()->empty());

  // Release locally.
  capture_controller->ReleaseCapture(top_level);
  EXPECT_EQ("OnCaptureChanged new_window=null old_window=0,11",
            SingleChangeToDescription(*(setup.changes())));
}

TEST(WindowTreeTest2, EventsGoToCaptureWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  top_level->AddChild(window);
  ASSERT_TRUE(top_level);
  top_level->Show();
  window->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetBounds(gfx::Rect(10, 10, 90, 90));
  // Left press on the top-level, leaving mouse down.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  event_generator.PressLeftButton();
  setup.window_tree_client()->ClearInputEvents();

  // Set capture on |window|.
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());

  // Move mouse, should go to |window|.
  event_generator.MoveMouseTo(6, 6);
  auto drag_event = setup.window_tree_client()->PopInputEvent();
  EXPECT_EQ(setup.window_tree_test_helper()->TransportIdForWindow(window),
            drag_event.window_id);
  EXPECT_EQ("POINTER_MOVED -4,-4",
            LocatedEventToEventTypeAndLocation(drag_event.event.get()));
}

TEST(WindowTreeTest2, InterceptEventsOnEmbeddedWindowWithCapture) {
  EventRecordingWindowDelegate window_delegate;
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  setup.delegate()->set_delegate_for_next_top_level(&window_delegate);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->AddChild(window);
  top_level->Show();
  window->Show();

  // Create an embedding, and a new window in the embedding.
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(window, mojom::kEmbedFlagEmbedderInterceptsEvents);
  ASSERT_TRUE(embedding_helper);
  aura::Window* window_in_child =
      embedding_helper->window_tree_test_helper->NewWindow();
  ASSERT_TRUE(window_in_child);
  window_in_child->Show();
  window->AddChild(window_in_child);
  EXPECT_TRUE(
      embedding_helper->window_tree_test_helper->SetCapture(window_in_child));

  // Do an initial move (which generates some additional events) and clear
  // everything out.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  setup.window_tree_client()->ClearInputEvents();
  window_delegate.ClearEvents();
  embedding_helper->window_tree_client.ClearInputEvents();

  // Move the mouse. Even though the window in the embedding has capture, the
  // event should go to the parent client (setup.window_tree_client()), because
  // the embedding was created such that the embedder (parent) intercepts the
  // events.
  event_generator.MoveMouseTo(6, 6);
  EXPECT_TRUE(window_delegate.events().empty());
  EXPECT_EQ("POINTER_MOVED",
            EventToEventType(
                setup.window_tree_client()->PopInputEvent().event.get()));
  EXPECT_TRUE(setup.window_tree_client()->input_events().empty());
  EXPECT_TRUE(embedding_helper->window_tree_client.input_events().empty());
}

TEST(WindowTreeTest2, PointerDownResetOnCaptureChange) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->AddChild(window);
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));
  top_level->Show();
  window->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetBounds(gfx::Rect(10, 10, 90, 90));
  // Left press on the top-level, leaving mouse down.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  event_generator.PressLeftButton();
  ServerWindow* top_level_server_window = ServerWindow::GetMayBeNull(top_level);
  ASSERT_TRUE(top_level_server_window);
  ServerWindowTestHelper top_level_server_window_helper(
      top_level_server_window);
  EXPECT_TRUE(top_level_server_window_helper.IsHandlingPointerPress(
      MouseEvent::kMousePointerId));

  // Set capture on |window|, top_level should no longer be in pointer-down
  // (because capture changed).
  EXPECT_TRUE(setup.window_tree_test_helper()->SetCapture(window));
  EXPECT_FALSE(top_level_server_window_helper.IsHandlingPointerPress(
      MouseEvent::kMousePointerId));
}

TEST(WindowTreeTest2, PointerDownResetOnHide) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  setup.window_tree_test_helper()->SetClientArea(top_level,
                                                 gfx::Insets(10, 0, 0, 0));
  top_level->Show();
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  // Left press on the top-level, leaving mouse down.
  test::EventGenerator event_generator(setup.root());
  event_generator.MoveMouseTo(5, 5);
  event_generator.PressLeftButton();
  ServerWindow* top_level_server_window = ServerWindow::GetMayBeNull(top_level);
  ASSERT_TRUE(top_level_server_window);
  ServerWindowTestHelper top_level_server_window_helper(
      top_level_server_window);
  EXPECT_TRUE(top_level_server_window_helper.IsHandlingPointerPress(
      MouseEvent::kMousePointerId));

  // Hiding should implicitly cancel capture.
  top_level->Hide();
  EXPECT_FALSE(top_level_server_window_helper.IsHandlingPointerPress(
      MouseEvent::kMousePointerId));
}

TEST(WindowTreeTest2, DeleteWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  aura::WindowTracker tracker;
  tracker.Add(window);
  setup.changes()->clear();
  setup.window_tree_test_helper()->DeleteWindow(window);
  EXPECT_TRUE(tracker.windows().empty());
  EXPECT_EQ("ChangeCompleted id=1 success=true",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest2, ExternalDeleteWindow) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  setup.changes()->clear();
  delete window;
  EXPECT_EQ("WindowDeleted window=0,1",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest2, Embed) {
  WindowServiceTestSetup setup;
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window);
  ASSERT_TRUE(embed_window);
  window->AddChild(embed_window);
  embed_window->SetBounds(gfx::Rect(1, 2, 3, 4));
  setup.changes()->clear();

  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(embed_window);
  ASSERT_TRUE(embedding_helper);
  ASSERT_EQ("OnEmbed", SingleChangeToDescription(*embedding_helper->changes()));
  const Change& test_change = (*embedding_helper->changes())[0];
  ASSERT_EQ(1u, test_change.windows.size());
  EXPECT_EQ(embed_window->bounds(), test_change.windows[0].bounds);
  EXPECT_EQ(kInvalidTransportId, test_change.windows[0].parent_id);
  EXPECT_EQ(embed_window->TargetVisibility(), test_change.windows[0].visible);
  EXPECT_NE(kInvalidTransportId, test_change.windows[0].window_id);

  // OnFrameSinkIdAllocated() should called on the parent tree.
  ASSERT_EQ(1u, setup.changes()->size());
  EXPECT_EQ(CHANGE_TYPE_FRAME_SINK_ID_ALLOCATED, (*setup.changes())[0].type);
}

// Base class for ScheduleEmbed() related tests. This creates a Window and
// prepares a secondary client (|embed_client_|) that is intended to be embedded
// at some point.
class WindowTreeScheduleEmbedTest : public testing::Test {
 public:
  WindowTreeScheduleEmbedTest() = default;
  ~WindowTreeScheduleEmbedTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    setup_ = std::make_unique<WindowServiceTestSetup>();
    embed_binding_.Bind(mojo::MakeRequest(&embed_client_ptr_));
    window_ = setup_->window_tree_test_helper()->NewWindow();
    ASSERT_TRUE(window_);
  }
  void TearDown() override {
    window_ = nullptr;
    embed_binding_.Close();
    setup_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<WindowServiceTestSetup> setup_;
  TestWindowTreeClient embed_client_;
  mojom::WindowTreeClientPtr embed_client_ptr_;
  aura::Window* window_ = nullptr;

 private:
  mojo::Binding<mojom::WindowTreeClient> embed_binding_{&embed_client_};

  DISALLOW_COPY_AND_ASSIGN(WindowTreeScheduleEmbedTest);
};

TEST_F(WindowTreeScheduleEmbedTest, ScheduleEmbedWithUnregisteredToken) {
  bool embed_result = false;
  bool embed_callback_called = false;
  setup_->window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup_->window_tree_test_helper()->TransportIdForWindow(window_),
      base::UnguessableToken::Create(), kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  // ScheduleEmbed() with an invalid token should fail.
  EXPECT_FALSE(embed_result);
}

TEST_F(WindowTreeScheduleEmbedTest, ScheduleEmbedRegisteredTokenInvalidWindow) {
  // Register a token for embedding.
  base::UnguessableToken token;
  setup_->window_tree_test_helper()->window_tree()->ScheduleEmbed(
      std::move(embed_client_ptr_),
      base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  bool embed_result = false;
  bool embed_callback_called = false;
  setup_->window_tree_test_helper()->window_tree()->EmbedUsingToken(
      kInvalidTransportId, token, kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  // ScheduleEmbed() with a valid token, but invalid window should fail.
  EXPECT_FALSE(embed_result);
}

TEST_F(WindowTreeScheduleEmbedTest, ScheduleEmbed) {
  base::UnguessableToken token;
  // ScheduleEmbed() with a valid token and valid window.
  setup_->window_tree_test_helper()->window_tree()->ScheduleEmbed(
      std::move(embed_client_ptr_),
      base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  bool embed_result = false;
  bool embed_callback_called = false;
  setup_->window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup_->window_tree_test_helper()->TransportIdForWindow(window_), token,
      kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);
  base::RunLoop().RunUntilIdle();

  // The embedded client should get OnEmbed().
  EXPECT_EQ("OnEmbed",
            SingleChangeToDescription(*embed_client_.tracker()->changes()));
}

TEST(WindowTreeTest2, ScheduleEmbedForExistingClient) {
  WindowServiceTestSetup setup;
  // Schedule an embed in the tree created by |setup|.
  base::UnguessableToken token;
  const uint32_t window_id_in_child = 149;
  setup.window_tree_test_helper()
      ->window_tree()
      ->ScheduleEmbedForExistingClient(
          window_id_in_child, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Create another client and a window.
  TestWindowTreeClient client2;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&client2);
  ASSERT_TRUE(tree2);
  WindowTreeTestHelper tree2_test_helper(tree2.get());
  aura::Window* window_in_parent = tree2_test_helper.NewWindow();
  ASSERT_TRUE(window_in_parent);

  // Call EmbedUsingToken() from tree2, which should result in the tree from
  // |setup| getting OnEmbedFromToken().
  bool embed_result = false;
  bool embed_callback_called = false;
  WindowTreeTestHelper(tree2.get())
      .window_tree()
      ->EmbedUsingToken(
          tree2_test_helper.TransportIdForWindow(window_in_parent), token,
          kDefaultEmbedFlags,
          base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                         &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);

  EXPECT_EQ("OnEmbedFromToken", SingleChangeToDescription(*setup.changes()));
  EXPECT_EQ(
      static_cast<Id>(window_id_in_child),
      setup.window_tree_test_helper()->TransportIdForWindow(window_in_parent));
}

TEST(WindowTreeTest2, DeleteRootOfEmbeddingFromScheduleEmbedForExistingClient) {
  WindowServiceTestSetup setup;
  aura::Window* window_in_parent = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window_in_parent);

  // Create another client.
  TestWindowTreeClient client2;
  std::unique_ptr<WindowTree> tree2 =
      setup.service()->CreateWindowTree(&client2);
  WindowTreeTestHelper tree2_test_helper(tree2.get());
  base::UnguessableToken token;
  tree2_test_helper.window_tree()->ScheduleEmbedForExistingClient(
      11, base::BindOnce(&ScheduleEmbedCallback, &token));
  EXPECT_FALSE(token.is_empty());

  // Call EmbedUsingToken() from setup.window_tree(), which should result in
  // |tree2| getting OnEmbedFromToken().
  bool embed_result = false;
  bool embed_callback_called = false;
  setup.window_tree_test_helper()->window_tree()->EmbedUsingToken(
      setup.window_tree_test_helper()->TransportIdForWindow(window_in_parent),
      token, kDefaultEmbedFlags,
      base::BindOnce(&EmbedUsingTokenCallback, &embed_callback_called,
                     &embed_result));
  EXPECT_TRUE(embed_callback_called);
  EXPECT_TRUE(embed_result);

  EXPECT_EQ("OnEmbedFromToken",
            SingleChangeToDescription(*client2.tracker()->changes()));
  client2.tracker()->changes()->clear();

  // Delete |window_in_parent|, which should trigger notifying tree2.
  setup.window_tree_test_helper()->DeleteWindow(window_in_parent);
  window_in_parent = nullptr;

  // 11 is the same value supplied to ScheduleEmbedForExistingClient().
  EXPECT_EQ("WindowDeleted window=0,11",
            SingleChangeToDescription(*client2.tracker()->changes()));
}

TEST(WindowTreeTest2, StackAtTop) {
  WindowServiceTestSetup setup;
  aura::Window* top_level1 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level1);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->StackAtTop(
      10, setup.window_tree_test_helper()->TransportIdForWindow(top_level1));
  // This succeeds because |top_level1| is already at top. |10| is the value
  // supplied to StackAtTop().
  EXPECT_EQ("ChangeCompleted id=10 success=true",
            SingleChangeToDescription(*setup.changes()));

  // Create another top-level. |top_level2| should initially be above 1.
  aura::Window* top_level2 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level2);
  ASSERT_EQ(2u, top_level1->parent()->children().size());
  EXPECT_EQ(top_level2, top_level1->parent()->children()[1]);

  // Stack 1 at the top.
  EXPECT_TRUE(setup.window_tree_test_helper()->StackAtTop(top_level1));
  EXPECT_EQ(top_level1, top_level1->parent()->children()[1]);

  // Stacking a non-toplevel window at top should fail.
  aura::Window* non_top_level_window =
      setup.window_tree_test_helper()->NewWindow();
  EXPECT_FALSE(
      setup.window_tree_test_helper()->StackAtTop(non_top_level_window));
}

TEST(WindowTreeTest2, OnUnhandledKeyEvent) {
  // Create a top-level, show it and give it focus.
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  top_level->Focus();
  ASSERT_TRUE(top_level->HasFocus());
  test::EventGenerator event_generator(setup.root());

  // Generate a key-press. The client should get the event, but not the
  // delegate.
  event_generator.PressKey(VKEY_A, EF_CONTROL_DOWN);
  EXPECT_TRUE(setup.delegate()->unhandled_key_events()->empty());

  // Respond that the event was not handled. Should result in notifying the
  // delegate.
  EXPECT_TRUE(setup.window_tree_client()->AckFirstEvent(
      setup.window_tree(), mojom::EventResult::UNHANDLED));
  ASSERT_EQ(1u, setup.delegate()->unhandled_key_events()->size());
  EXPECT_EQ(VKEY_A, (*setup.delegate()->unhandled_key_events())[0].key_code());
  EXPECT_EQ(EF_CONTROL_DOWN,
            (*setup.delegate()->unhandled_key_events())[0].flags());
  setup.delegate()->unhandled_key_events()->clear();

  // Repeat, but respond with handled. This should not result in the delegate
  // being notified.
  event_generator.PressKey(VKEY_B, EF_SHIFT_DOWN);
  EXPECT_TRUE(setup.window_tree_client()->AckFirstEvent(
      setup.window_tree(), mojom::EventResult::HANDLED));
  EXPECT_TRUE(setup.delegate()->unhandled_key_events()->empty());
}

TEST(WindowTreeTest2, ReorderWindow) {
  // Create a top-level and two child windows.
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  aura::Window* window1 = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window1);
  top_level->AddChild(window1);
  aura::Window* window2 = setup.window_tree_test_helper()->NewWindow();
  ASSERT_TRUE(window2);
  top_level->AddChild(window2);

  // Reorder |window1| on top of |window2|.
  EXPECT_TRUE(setup.window_tree_test_helper()->ReorderWindow(
      window1, window2, mojom::OrderDirection::ABOVE));
  EXPECT_EQ(window2, top_level->children()[0]);
  EXPECT_EQ(window1, top_level->children()[1]);

  // Reorder |window2| on top of |window1|.
  EXPECT_TRUE(setup.window_tree_test_helper()->ReorderWindow(
      window2, window1, mojom::OrderDirection::ABOVE));
  EXPECT_EQ(window1, top_level->children()[0]);
  EXPECT_EQ(window2, top_level->children()[1]);

  // Repeat, but use the WindowTree interface, which should result in an ack.
  setup.changes()->clear();
  uint32_t change_id = 101;
  setup.window_tree_test_helper()->window_tree()->ReorderWindow(
      change_id, setup.window_tree_test_helper()->TransportIdForWindow(window1),
      setup.window_tree_test_helper()->TransportIdForWindow(window2),
      mojom::OrderDirection::ABOVE);
  EXPECT_EQ("ChangeCompleted id=101 success=true",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  // Supply invalid window ids, which should fail.
  setup.window_tree_test_helper()->window_tree()->ReorderWindow(
      change_id, 0, 1, mojom::OrderDirection::ABOVE);
  EXPECT_EQ("ChangeCompleted id=101 success=false",
            SingleChangeToDescription(*setup.changes()));

  // These calls should fail as the windows are not siblings.
  EXPECT_FALSE(setup.window_tree_test_helper()->ReorderWindow(
      window1, top_level, mojom::OrderDirection::ABOVE));
  EXPECT_FALSE(setup.window_tree_test_helper()->ReorderWindow(
      top_level, window2, mojom::OrderDirection::ABOVE));
}

TEST(WindowTreeTest2, StackAbove) {
  // Create two top-levels.
  WindowServiceTestSetup setup;
  aura::Window* top_level1 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level1);
  aura::Window* top_level2 =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level2);
  ASSERT_TRUE(top_level1->parent());
  ASSERT_EQ(top_level1->parent(), top_level2->parent());
  ASSERT_EQ(2u, top_level2->parent()->children().size());

  // 1 on top of 2.
  EXPECT_TRUE(
      setup.window_tree_test_helper()->StackAbove(top_level1, top_level2));
  EXPECT_EQ(top_level2, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level1, top_level2->parent()->children()[1]);

  // Repeat, should still succeed and nothing should change.
  EXPECT_TRUE(
      setup.window_tree_test_helper()->StackAbove(top_level1, top_level2));
  EXPECT_EQ(top_level2, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level1, top_level2->parent()->children()[1]);

  // 2 on top of 1.
  EXPECT_TRUE(
      setup.window_tree_test_helper()->StackAbove(top_level2, top_level1));
  EXPECT_EQ(top_level1, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level2, top_level2->parent()->children()[1]);

  // 1 on top of 2, using WindowTree interface, which should result in an ack.
  setup.changes()->clear();
  uint32_t change_id = 102;
  setup.window_tree_test_helper()->window_tree()->StackAbove(
      change_id,
      setup.window_tree_test_helper()->TransportIdForWindow(top_level1),
      setup.window_tree_test_helper()->TransportIdForWindow(top_level2));
  EXPECT_EQ("ChangeCompleted id=102 success=true",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();
  EXPECT_EQ(top_level2, top_level2->parent()->children()[0]);
  EXPECT_EQ(top_level1, top_level2->parent()->children()[1]);

  // Using invalid id should fail.
  setup.window_tree_test_helper()->window_tree()->StackAbove(
      change_id,
      setup.window_tree_test_helper()->TransportIdForWindow(top_level1),
      kInvalidTransportId);
  EXPECT_EQ("ChangeCompleted id=102 success=false",
            SingleChangeToDescription(*setup.changes()));

  // Using non-top-level should fail.
  aura::Window* non_top_level_window =
      setup.window_tree_test_helper()->NewWindow();
  EXPECT_FALSE(setup.window_tree_test_helper()->StackAbove(
      top_level1, non_top_level_window));
}

TEST(WindowTreeTest2, RunMoveLoopTouch) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      12, top_level_id, mojom::MoveLoopSource::TOUCH, gfx::Point());
  // |top_level| isn't visible, so should fail immediately.
  EXPECT_EQ("ChangeCompleted id=12 success=false",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  // Make the window visible and repeat.
  top_level->Show();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      13, top_level_id, mojom::MoveLoopSource::TOUCH, gfx::Point());
  // WindowServiceDelegate should be asked to do the move.
  WindowServiceDelegate::DoneCallback move_loop_callback =
      setup.delegate()->TakeMoveLoopCallback();
  ASSERT_TRUE(move_loop_callback);
  // As the move is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Respond to the callback with success, which should notify client.
  std::move(move_loop_callback).Run(true);
  EXPECT_EQ("ChangeCompleted id=13 success=true",
            SingleChangeToDescription(*setup.changes()));

  // Trying to move non-top-level should fail.
  aura::Window* non_top_level_window =
      setup.window_tree_test_helper()->NewWindow();
  non_top_level_window->Show();
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      14,
      setup.window_tree_test_helper()->TransportIdForWindow(
          non_top_level_window),
      mojom::MoveLoopSource::TOUCH, gfx::Point());
  EXPECT_EQ("ChangeCompleted id=14 success=false",
            SingleChangeToDescription(*setup.changes()));
}

TEST(WindowTreeTest2, RunMoveLoopMouse) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      12, top_level_id, mojom::MoveLoopSource::MOUSE, gfx::Point());
  // The mouse isn't down, so this should fail.
  EXPECT_EQ("ChangeCompleted id=12 success=false",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();

  // Press the left button and repeat.
  test::EventGenerator event_generator(setup.root());
  event_generator.PressLeftButton();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      13, top_level_id, mojom::MoveLoopSource::MOUSE, gfx::Point());
  // WindowServiceDelegate should be asked to do the move.
  WindowServiceDelegate::DoneCallback move_loop_callback =
      setup.delegate()->TakeMoveLoopCallback();
  ASSERT_TRUE(move_loop_callback);
  // As the move is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Respond to the callback, which should notify client.
  std::move(move_loop_callback).Run(true);
  EXPECT_EQ("ChangeCompleted id=13 success=true",
            SingleChangeToDescription(*setup.changes()));
  setup.changes()->clear();
}

TEST(WindowTreeTest2, CancelMoveLoop) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  top_level->Show();
  const Id top_level_id =
      setup.window_tree_test_helper()->TransportIdForWindow(top_level);
  setup.changes()->clear();
  setup.window_tree_test_helper()->window_tree()->PerformWindowMove(
      12, top_level_id, mojom::MoveLoopSource::TOUCH, gfx::Point());

  // WindowServiceDelegate should be asked to do the move.
  WindowServiceDelegate::DoneCallback move_loop_callback =
      setup.delegate()->TakeMoveLoopCallback();
  ASSERT_TRUE(move_loop_callback);
  // As the move is in progress, changes should be empty.
  EXPECT_TRUE(setup.changes()->empty());

  // Cancelling with an invalid id should do nothing.
  EXPECT_FALSE(setup.delegate()->cancel_window_move_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelWindowMove(
      kInvalidTransportId);
  EXPECT_TRUE(setup.changes()->empty());
  EXPECT_FALSE(setup.delegate()->cancel_window_move_loop_called());

  // Cancel with the real id should notify the delegate.
  EXPECT_FALSE(setup.delegate()->cancel_window_move_loop_called());
  setup.window_tree_test_helper()->window_tree()->CancelWindowMove(
      top_level_id);
  EXPECT_TRUE(setup.delegate()->cancel_window_move_loop_called());
  // No changes yet, because |move_loop_callback| was not run yet.
  EXPECT_TRUE(setup.changes()->empty());
  // Run the closure, which triggers notifying the client.
  std::move(move_loop_callback).Run(false);
  EXPECT_EQ("ChangeCompleted id=12 success=false",
            SingleChangeToDescription(*setup.changes()));
}

}  // namespace
}  // namespace ws2
}  // namespace ui
