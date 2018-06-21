// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/ozone_platform_scenic.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/display/manager/fake_display_delegate.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/system_input_injector.h"
#include "ui/ozone/common/stub_overlay_manager.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

class ScenicPlatformEventSource : public ui::PlatformEventSource {
 public:
  ScenicPlatformEventSource() = default;
  ~ScenicPlatformEventSource() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScenicPlatformEventSource);
};

// OzonePlatform for Scenic.
class OzonePlatformScenic : public OzonePlatform {
 public:
  OzonePlatformScenic() = default;
  ~OzonePlatformScenic() override = default;

  ScenicWindowManager* window_manager() { return &window_manager_; }

  // OzonePlatform implementation.
  ui::SurfaceFactoryOzone* GetSurfaceFactoryOzone() override {
    return &surface_factory_;
  }

  OverlayManagerOzone* GetOverlayManager() override {
    return overlay_manager_.get();
  }

  CursorFactoryOzone* GetCursorFactoryOzone() override {
    return cursor_factory_ozone_.get();
  }

  InputController* GetInputController() override {
    return input_controller_.get();
  }

  GpuPlatformSupportHost* GetGpuPlatformSupportHost() override {
    return gpu_platform_support_host_.get();
  }

  std::unique_ptr<SystemInputInjector> CreateSystemInputInjector() override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  std::unique_ptr<PlatformWindow> CreatePlatformWindow(
      PlatformWindowDelegate* delegate,
      PlatformWindowInitProperties properties) override {
    if (!properties.view_owner_request) {
      NOTREACHED();
      return nullptr;
    }
    return std::make_unique<ScenicWindow>(
        &window_manager_, delegate, std::move(properties.view_owner_request));
  }

  std::unique_ptr<display::NativeDisplayDelegate> CreateNativeDisplayDelegate()
      override {
    NOTIMPLEMENTED();
    return std::make_unique<display::FakeDisplayDelegate>();
  }

  void InitializeUI(const InitParams& params) override {
    if (!PlatformEventSource::GetInstance())
      platform_event_source_ = std::make_unique<ScenicPlatformEventSource>();
    KeyboardLayoutEngineManager::SetKeyboardLayoutEngine(
        std::make_unique<StubKeyboardLayoutEngine>());

    overlay_manager_ = std::make_unique<StubOverlayManager>();
    input_controller_ = CreateStubInputController();
    cursor_factory_ozone_ = std::make_unique<BitmapCursorFactoryOzone>();
    gpu_platform_support_host_.reset(CreateStubGpuPlatformSupportHost());
  }

  void InitializeGPU(const InitParams& params) override {}

 private:
  ScenicWindowManager window_manager_;
  ScenicSurfaceFactory surface_factory_;

  std::unique_ptr<PlatformEventSource> platform_event_source_;
  std::unique_ptr<CursorFactoryOzone> cursor_factory_ozone_;
  std::unique_ptr<InputController> input_controller_;
  std::unique_ptr<GpuPlatformSupportHost> gpu_platform_support_host_;
  std::unique_ptr<OverlayManagerOzone> overlay_manager_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatformScenic);
};

}  // namespace

OzonePlatform* CreateOzonePlatformScenic() {
  return new OzonePlatformScenic();
}

}  // namespace ui
