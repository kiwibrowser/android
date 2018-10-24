// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_DRAG_DROP_DELEGATE_H_
#define SERVICES_UI_WS2_DRAG_DROP_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws2/ids.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/base/dragdrop/drag_drop_types.h"

namespace ui {
namespace ws2 {

// A delegate to forward drag and drop events to a remote client window via
// mojom::WindowTreeClient.
class DragDropDelegate : public aura::client::DragDropDelegate {
 public:
  DragDropDelegate(mojom::WindowTreeClient* window_tree_client,
                   aura::Window* window,
                   Id transport_window_id);
  ~DragDropDelegate() override;

  // aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

 private:
  void StartDrag(const ui::DropTargetEvent& event);
  void EndDrag();

  // Callback invoked to update |last_drag_operations_|.
  void UpdateDragOperations(uint32_t drag_operations);

  mojom::WindowTreeClient* const tree_client_;
  aura::Window* const window_;
  const Id transport_window_id_;

  // Whether a drag is over |window_|.
  bool in_drag_ = false;

  // Cached drag operations as a workaround to return drag operations for
  // synchronous OnDragUpdated.
  uint32_t last_drag_operations_ = ui::DragDropTypes::DRAG_NONE;

  base::WeakPtrFactory<DragDropDelegate> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DragDropDelegate);
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_DRAG_DROP_DELEGATE_H_
