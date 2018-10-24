// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_port_mash.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_registrar.h"
#include "ash/display/display_synchronizer.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/ash_window_tree_host_mus.h"
#include "ash/keyboard/keyboard_ui_mash.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/touch/touch_transform_setter_mus.h"
#include "ash/window_manager.h"
#include "ash/wm/drag_window_resizer_mash.h"
#include "ash/wm/immersive_handler_factory_mash.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler.h"
#include "ash/wm/window_cycle_event_filter.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler_mash.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/public/interfaces/video_detector.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/forwarding_display_delegate.h"
#include "ui/display/mojo/native_display_delegate.mojom.h"
#include "ui/views/mus/pointer_watcher_event_router.h"

namespace ash {

ShellPortMash::ShellPortMash(
    WindowManager* window_manager,
    views::PointerWatcherEventRouter* pointer_watcher_event_router)
    : window_manager_(window_manager),
      pointer_watcher_event_router_(pointer_watcher_event_router),
      immersive_handler_factory_(
          std::make_unique<ImmersiveHandlerFactoryMash>()) {
  DCHECK(window_manager_);
  DCHECK(pointer_watcher_event_router_);
  DCHECK_EQ(Config::MASH, GetAshConfig());
}

ShellPortMash::~ShellPortMash() = default;

// static
ShellPortMash* ShellPortMash::Get() {
  const ash::Config config = ShellPort::Get()->GetAshConfig();
  CHECK_EQ(Config::MASH, config);
  return static_cast<ShellPortMash*>(ShellPort::Get());
}

void ShellPortMash::Shutdown() {
  display_synchronizer_.reset();
  ShellPort::Shutdown();
}

Config ShellPortMash::GetAshConfig() const {
  return Config::MASH;
}

std::unique_ptr<display::TouchTransformSetter>
ShellPortMash::CreateTouchTransformDelegate() {
  return std::make_unique<TouchTransformSetterMus>(
      window_manager_->connector());
}

void ShellPortMash::LockCursor() {
  // When we are running in mus, we need to keep track of state not just in the
  // window server, but also locally in ash because ash treats the cursor
  // manager as the canonical state for now. NativeCursorManagerAsh will keep
  // this state, while also forwarding it to the window manager for us.
  window_manager_->window_manager_client()->LockCursor();
}

void ShellPortMash::UnlockCursor() {
  window_manager_->window_manager_client()->UnlockCursor();
}

void ShellPortMash::ShowCursor() {
  window_manager_->window_manager_client()->SetCursorVisible(true);
}

void ShellPortMash::HideCursor() {
  window_manager_->window_manager_client()->SetCursorVisible(false);
}

void ShellPortMash::SetCursorSize(ui::CursorSize cursor_size) {
  window_manager_->window_manager_client()->SetCursorSize(cursor_size);
}

void ShellPortMash::SetGlobalOverrideCursor(
    base::Optional<ui::CursorData> cursor) {
  window_manager_->window_manager_client()->SetGlobalOverrideCursor(
      std::move(cursor));
}

bool ShellPortMash::IsMouseEventsEnabled() {
  return cursor_touch_visible_;
}

void ShellPortMash::SetCursorTouchVisible(bool enabled) {
  window_manager_->window_manager_client()->SetCursorTouchVisible(enabled);
}

void ShellPortMash::OnCursorTouchVisibleChanged(bool enabled) {
  cursor_touch_visible_ = enabled;
}

std::unique_ptr<WindowResizer> ShellPortMash::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return std::make_unique<ash::DragWindowResizerMash>(
      std::move(next_window_resizer), window_state);
}

std::unique_ptr<WindowCycleEventFilter>
ShellPortMash::CreateWindowCycleEventFilter() {
  // TODO: implement me, http://crbug.com/629191.
  return nullptr;
}

std::unique_ptr<wm::TabletModeEventHandler>
ShellPortMash::CreateTabletModeEventHandler() {
  // TODO: need support for window manager to get events before client:
  // http://crbug.com/624157.
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<WorkspaceEventHandler>
ShellPortMash::CreateWorkspaceEventHandler(aura::Window* workspace_window) {
  return std::make_unique<WorkspaceEventHandlerMash>(workspace_window);
}

std::unique_ptr<KeyboardUI> ShellPortMash::CreateKeyboardUI() {
  return KeyboardUIMash::Create(window_manager_->connector());
}

void ShellPortMash::AddPointerWatcher(views::PointerWatcher* watcher,
                                      views::PointerWatcherEventTypes events) {
  // TODO: implement drags for mus pointer watcher, http://crbug.com/641164.
  // NOTIMPLEMENTED_LOG_ONCE drags for mus pointer watcher.
  pointer_watcher_event_router_->AddPointerWatcher(
      watcher, events == views::PointerWatcherEventTypes::MOVES);
}

void ShellPortMash::RemovePointerWatcher(views::PointerWatcher* watcher) {
  pointer_watcher_event_router_->RemovePointerWatcher(watcher);
}

bool ShellPortMash::IsTouchDown() {
  // TODO: implement me, http://crbug.com/634967.
  return false;
}

void ShellPortMash::ToggleIgnoreExternalKeyboard() {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ShellPortMash::CreatePointerWatcherAdapter() {
  // In Config::CLASSIC PointerWatcherAdapterClassic must be created when this
  // function is called (it is order dependent), that is not the case with
  // Config::MASH.
}

std::unique_ptr<AshWindowTreeHost> ShellPortMash::CreateAshWindowTreeHost(
    const AshWindowTreeHostInitParams& init_params) {
  auto display_params = std::make_unique<aura::DisplayInitParams>();
  display_params->viewport_metrics.bounds_in_pixels =
      init_params.initial_bounds;
  display_params->viewport_metrics.device_scale_factor =
      init_params.device_scale_factor;
  display_params->viewport_metrics.ui_scale_factor =
      init_params.ui_scale_factor;
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::Display mirrored_display =
      display_manager->GetMirroringDisplayById(init_params.display_id);
  if (mirrored_display.is_valid()) {
    display_params->display =
        std::make_unique<display::Display>(mirrored_display);
  }
  display_params->is_primary_display = true;
  display_params->mirrors = display_manager->software_mirroring_display_list();
  aura::WindowTreeHostMusInitParams aura_init_params =
      window_manager_->window_manager_client()->CreateInitParamsForNewDisplay();
  aura_init_params.display_id = init_params.display_id;
  aura_init_params.display_init_params = std::move(display_params);
  aura_init_params.use_classic_ime = !Shell::ShouldUseIMEService();
  return std::make_unique<AshWindowTreeHostMus>(std::move(aura_init_params));
}

void ShellPortMash::OnCreatedRootWindowContainers(
    RootWindowController* root_window_controller) {
  // TODO: To avoid lots of IPC AddActivationParent() should take an array.
  // http://crbug.com/682048.
  aura::Window* root_window = root_window_controller->GetRootWindow();
  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_->window_manager_client()->AddActivationParent(
        root_window->GetChildById(kActivatableShellWindowIds[i]));
  }

  UpdateSystemModalAndBlockingContainers();
}

void ShellPortMash::UpdateSystemModalAndBlockingContainers() {
  std::vector<aura::BlockingContainers> all_blocking_containers;
  for (RootWindowController* root_window_controller :
       Shell::GetAllRootWindowControllers()) {
    aura::BlockingContainers blocking_containers;
    wm::GetBlockingContainersForRoot(
        root_window_controller->GetRootWindow(),
        &blocking_containers.min_container,
        &blocking_containers.system_modal_container);
    all_blocking_containers.push_back(blocking_containers);
  }
  window_manager_->window_manager_client()->SetBlockingContainers(
      all_blocking_containers);
}

void ShellPortMash::OnHostsInitialized() {
  display_synchronizer_ = std::make_unique<DisplaySynchronizer>(
      window_manager_->window_manager_client());
}

std::unique_ptr<display::NativeDisplayDelegate>
ShellPortMash::CreateNativeDisplayDelegate() {
  display::mojom::NativeDisplayDelegatePtr native_display_delegate;
  if (window_manager_->connector()) {
    window_manager_->connector()->BindInterface(ui::mojom::kServiceName,
                                                &native_display_delegate);
  }
  return std::make_unique<display::ForwardingDisplayDelegate>(
      std::move(native_display_delegate));
}

std::unique_ptr<AcceleratorController>
ShellPortMash::CreateAcceleratorController() {
  uint16_t accelerator_namespace_id = 0u;
  const bool add_result =
      window_manager_->GetNextAcceleratorNamespaceId(&accelerator_namespace_id);
  // ShellPortMash is created early on, so that GetNextAcceleratorNamespaceId()
  // should always succeed.
  DCHECK(add_result);

  accelerator_controller_registrar_ =
      std::make_unique<AcceleratorControllerRegistrar>(
          window_manager_, accelerator_namespace_id);
  return std::make_unique<AcceleratorController>(
      accelerator_controller_registrar_.get());
}

void ShellPortMash::AddVideoDetectorObserver(
    viz::mojom::VideoDetectorObserverPtr observer) {
  // We may not have access to the connector in unit tests.
  if (!window_manager_->connector())
    return;

  ui::mojom::VideoDetectorPtr video_detector;
  window_manager_->connector()->BindInterface(ui::mojom::kServiceName,
                                              &video_detector);
  video_detector->AddObserver(std::move(observer));
}

}  // namespace ash
