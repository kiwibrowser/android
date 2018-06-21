// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_LAYOUT_MANAGER_H_
#define UI_KEYBOARD_KEYBOARD_LAYOUT_MANAGER_H_

#include "base/macros.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_export.h"

namespace keyboard {

class KeyboardController;

// LayoutManager for the virtual keyboard container. Manages a single window
// (the virtual keyboard) and keeps it positioned at the bottom of the
// owner window.
class KEYBOARD_EXPORT KeyboardLayoutManager : public aura::LayoutManager {
 public:
  explicit KeyboardLayoutManager(KeyboardController* controller);
  ~KeyboardLayoutManager() override;

  // Overridden from aura::LayoutManager
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  KeyboardController* controller_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardLayoutManager);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_LAYOUT_MANAGER_H_
