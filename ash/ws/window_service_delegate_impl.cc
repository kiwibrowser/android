// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_delegate_impl.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/non_client_frame_controller.h"
#include "ash/wm/top_level_window_factory.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/compound_event_filter.h"

namespace ash {
namespace {

// Function supplied to WmToplevelWindowEventHandler::AttemptToStartDrag().
// |end_closure| is the callback that was supplied to RunWindowMoveLoop().
void OnMoveLoopCompleted(base::OnceCallback<void(bool success)> end_closure,
                         wm::WmToplevelWindowEventHandler::DragResult result) {
  std::move(end_closure)
      .Run(result == wm::WmToplevelWindowEventHandler::DragResult::SUCCESS);
}

// Returns true if a window move loop can be started.
bool ShouldStartMoveLoop(aura::Window* window,
                         ui::mojom::MoveLoopSource source) {
  // A window move can not be started while drag and drop is in progress.
  aura::client::DragDropClient* drag_drop_client =
      aura::client::GetDragDropClient(window->GetRootWindow());
  if (drag_drop_client && drag_drop_client->IsDragDropInProgress())
    return false;

  ToplevelWindowEventHandler* event_handler =
      Shell::Get()->toplevel_window_event_handler();
  // Only one move is allowed at a time.
  if (event_handler->wm_toplevel_window_event_handler()
          ->is_drag_in_progress()) {
    return false;
  }
  return true;
}

}  // namespace

WindowServiceDelegateImpl::WindowServiceDelegateImpl() = default;

WindowServiceDelegateImpl::~WindowServiceDelegateImpl() = default;

std::unique_ptr<aura::Window> WindowServiceDelegateImpl::NewTopLevel(
    aura::PropertyConverter* property_converter,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  std::map<std::string, std::vector<uint8_t>> property_map =
      mojo::FlatMapToMap(properties);
  ui::mojom::WindowType window_type =
      aura::GetWindowTypeFromProperties(property_map);

  auto* window =
      CreateAndParentTopLevelWindow(nullptr /* window_manager */, window_type,
                                    property_converter, &property_map);
  return base::WrapUnique<aura::Window>(window);
}

void WindowServiceDelegateImpl::OnUnhandledKeyEvent(
    const ui::KeyEvent& key_event) {
  Shell::Get()->accelerator_controller()->Process(ui::Accelerator(key_event));
}

bool WindowServiceDelegateImpl::StoreAndSetCursor(aura::Window* window,
                                                  ui::Cursor cursor) {
  auto* frame = NonClientFrameController::Get(window);
  if (frame)
    frame->StoreCursor(cursor);

  ash::Shell::Get()->env_filter()->SetCursorForWindow(window, cursor);

  return !!frame;
}

void WindowServiceDelegateImpl::RunWindowMoveLoop(
    aura::Window* window,
    ui::mojom::MoveLoopSource source,
    const gfx::Point& cursor,
    DoneCallback callback) {
  if (!ShouldStartMoveLoop(window, source)) {
    std::move(callback).Run(false);
    return;
  }

  if (source == ui::mojom::MoveLoopSource::MOUSE)
    window->SetCapture();

  const ::wm::WindowMoveSource aura_source =
      source == ui::mojom::MoveLoopSource::MOUSE
          ? ::wm::WINDOW_MOVE_SOURCE_MOUSE
          : ::wm::WINDOW_MOVE_SOURCE_TOUCH;
  Shell::Get()
      ->toplevel_window_event_handler()
      ->wm_toplevel_window_event_handler()
      ->AttemptToStartDrag(
          window, cursor, HTCAPTION, aura_source,
          base::BindOnce(&OnMoveLoopCompleted, std::move(callback)));
}

void WindowServiceDelegateImpl::CancelWindowMoveLoop() {
  Shell::Get()
      ->toplevel_window_event_handler()
      ->wm_toplevel_window_event_handler()
      ->RevertDrag();
}

}  // namespace ash
