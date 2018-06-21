// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_SERVICE_DELEGATE_H_
#define SERVICES_UI_WS2_WINDOW_SERVICE_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "ui/base/cursor/cursor.h"

namespace aura {
class PropertyConverter;
class Window;
}

namespace gfx {
class Point;
}

namespace ui {

class KeyEvent;

namespace ws2 {

// A delegate used by the WindowService for context-specific operations.
class COMPONENT_EXPORT(WINDOW_SERVICE) WindowServiceDelegate {
 public:
  // A client requested a new top-level window. Implementations should create a
  // new window, parenting it in the appropriate container. Return null to
  // reject the request.
  // NOTE: it is recommended that when clients create a new window they use
  // WindowDelegateImpl as the WindowDelegate of the Window (this must be done
  // by the WindowServiceDelegate, as the Window's delegate can not be changed
  // after creation).
  virtual std::unique_ptr<aura::Window> NewTopLevel(
      aura::PropertyConverter* property_converter,
      const base::flat_map<std::string, std::vector<uint8_t>>& properties) = 0;

  // Called for KeyEvents the client does not handle.
  virtual void OnUnhandledKeyEvent(const KeyEvent& key_event) {}

  // Sets the cursor for |window| to |cursor|. This will immediately change the
  // actual on-screen cursor if the pointer is hovered over |window|. Also store
  // |cursor| on the widget for |window| if there is one. The return value
  // indicates whether the cursor was stored for |window|.
  virtual bool StoreAndSetCursor(aura::Window* window, ui::Cursor cursor);

  // Called to start a move operation on |window|. When done, |callback| should
  // be run with the result (true if the move was successful). If a move is not
  // allowed, the delegate should run |callback| immediately.
  using DoneCallback = base::OnceCallback<void(bool)>;
  virtual void RunWindowMoveLoop(aura::Window* window,
                                 mojom::MoveLoopSource source,
                                 const gfx::Point& cursor,
                                 DoneCallback callback);

  // Called to cancel an in-progress window move loop that was started by
  // RunWindowMoveLoop().
  virtual void CancelWindowMoveLoop() {}

 protected:
  virtual ~WindowServiceDelegate() = default;
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_SERVICE_DELEGATE_H_
