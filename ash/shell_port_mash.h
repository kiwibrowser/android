// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PORT_MASH_H_
#define ASH_SHELL_PORT_MASH_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/shell_port.h"
#include "base/macros.h"

namespace views {
class PointerWatcherEventRouter;
}

namespace ash {

class AcceleratorControllerRegistrar;
class DisplaySynchronizer;
class ImmersiveHandlerFactoryMash;
class WindowManager;

// ShellPort implementation for mash. See ash/README.md for more.
class ShellPortMash : public ShellPort {
 public:
  ShellPortMash(WindowManager* window_manager,
                views::PointerWatcherEventRouter* pointer_watcher_event_router);
  ~ShellPortMash() override;

  static ShellPortMash* Get();

  // Called when the window server has changed the mouse enabled state.
  void OnCursorTouchVisibleChanged(bool enabled);

  // ShellPort:
  void Shutdown() override;
  Config GetAshConfig() const override;
  std::unique_ptr<display::TouchTransformSetter> CreateTouchTransformDelegate()
      override;
  void LockCursor() override;
  void UnlockCursor() override;
  void ShowCursor() override;
  void HideCursor() override;
  void SetCursorSize(ui::CursorSize cursor_size) override;
  void SetGlobalOverrideCursor(base::Optional<ui::CursorData> cursor) override;
  bool IsMouseEventsEnabled() override;
  void SetCursorTouchVisible(bool enabled) override;
  std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) override;
  std::unique_ptr<WindowCycleEventFilter> CreateWindowCycleEventFilter()
      override;
  std::unique_ptr<wm::TabletModeEventHandler> CreateTabletModeEventHandler()
      override;
  std::unique_ptr<WorkspaceEventHandler> CreateWorkspaceEventHandler(
      aura::Window* workspace_window) override;
  std::unique_ptr<KeyboardUI> CreateKeyboardUI() override;
  void AddPointerWatcher(views::PointerWatcher* watcher,
                         views::PointerWatcherEventTypes events) override;
  void RemovePointerWatcher(views::PointerWatcher* watcher) override;
  bool IsTouchDown() override;
  void ToggleIgnoreExternalKeyboard() override;
  void CreatePointerWatcherAdapter() override;
  std::unique_ptr<AshWindowTreeHost> CreateAshWindowTreeHost(
      const AshWindowTreeHostInitParams& init_params) override;
  void OnCreatedRootWindowContainers(
      RootWindowController* root_window_controller) override;
  void UpdateSystemModalAndBlockingContainers() override;
  void OnHostsInitialized() override;
  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override;
  std::unique_ptr<AcceleratorController> CreateAcceleratorController() override;
  void AddVideoDetectorObserver(
      viz::mojom::VideoDetectorObserverPtr observer) override;

 private:
  WindowManager* window_manager_;
  std::unique_ptr<DisplaySynchronizer> display_synchronizer_;
  views::PointerWatcherEventRouter* pointer_watcher_event_router_ = nullptr;
  std::unique_ptr<AcceleratorControllerRegistrar>
      accelerator_controller_registrar_;
  std::unique_ptr<ImmersiveHandlerFactoryMash> immersive_handler_factory_;

  bool cursor_touch_visible_ = true;

  DISALLOW_COPY_AND_ASSIGN(ShellPortMash);
};

}  // namespace ash

#endif  // ASH_SHELL_PORT_MASH_H_
