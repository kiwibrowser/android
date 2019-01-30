// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_owner.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/non_client_frame_controller.h"
#include "ash/ws/window_service_delegate_impl.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/ws2/gpu_interface_provider.h"
#include "services/ui/ws2/window_service.h"
#include "ui/wm/core/focus_controller.h"

namespace ash {

WindowServiceOwner::WindowServiceOwner(
    std::unique_ptr<ui::ws2::GpuInterfaceProvider> gpu_interface_provider)
    : gpu_interface_provider_(std::move(gpu_interface_provider)) {}

WindowServiceOwner::~WindowServiceOwner() = default;

void WindowServiceOwner::BindWindowService(
    service_manager::mojom::ServiceRequest request) {
  // This should only be called once. If called more than once it means the
  // WindowService lost its connection to the service_manager, which triggered
  // a new connection WindowService to be created. That should never happen.
  DCHECK(!service_context_);

  window_service_delegate_ = std::make_unique<WindowServiceDelegateImpl>();
  std::unique_ptr<ui::ws2::WindowService> window_service =
      std::make_unique<ui::ws2::WindowService>(
          window_service_delegate_.get(), std::move(gpu_interface_provider_),
          Shell::Get()->focus_controller());
  window_service_ = window_service.get();
  window_service_->SetFrameDecorationValues(
      NonClientFrameController::GetPreferredClientAreaInsets(),
      NonClientFrameController::GetMaxTitleBarButtonWidth());
  RegisterWindowProperties(window_service_->property_converter());
  service_context_ = std::make_unique<service_manager::ServiceContext>(
      std::move(window_service), std::move(request));
}

}  // namespace ash
