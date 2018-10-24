// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_GPU_INTERFACE_PROVIDER_H_
#define SERVICES_UI_WS2_GPU_INTERFACE_PROVIDER_H_

#include "base/component_export.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace ui {
namespace ws2 {

// GpuInterfaceProvider is responsible for providing the Gpu related interfaces.
// The implementation of these varies depending upon where the WindowService is
// hosted.
class COMPONENT_EXPORT(WINDOW_SERVICE) GpuInterfaceProvider {
 public:
  virtual ~GpuInterfaceProvider() {}

  // Registers the gpu-related interfaces, specifically
  // discardable_memory::mojom::DiscardableSharedMemoryManagerRequest and
  // ui::mojom::GpuRequest.
  virtual void RegisterGpuInterfaces(
      service_manager::BinderRegistry* registry) = 0;
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_GPU_INTERFACE_PROVIDER_H_
