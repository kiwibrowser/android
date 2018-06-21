// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_ASH_GPU_INTERFACE_PROVIDER_H_
#define ASH_WS_ASH_GPU_INTERFACE_PROVIDER_H_

#include "components/discardable_memory/public/interfaces/discardable_shared_memory_manager.mojom.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "services/ui/ws2/gpu_interface_provider.h"

namespace discardable_memory {
class DiscardableSharedMemoryManager;
}

namespace ui {
namespace gpu_host {
class GpuHost;
}
}  // namespace ui

namespace ash {

// Implementation of GpuInterfaceProvider used when Ash runs out of process.
class AshGpuInterfaceProvider : public ui::ws2::GpuInterfaceProvider {
 public:
  AshGpuInterfaceProvider(ui::gpu_host::GpuHost* gpu_host,
                          discardable_memory::DiscardableSharedMemoryManager*
                              discardable_shared_memory_manager);
  ~AshGpuInterfaceProvider() override;

  // ui::ws2::GpuInterfaceProvider:
  void RegisterGpuInterfaces(
      service_manager::BinderRegistry* registry) override;

 private:
  void BindDiscardableSharedMemoryManagerRequest(
      discardable_memory::mojom::DiscardableSharedMemoryManagerRequest request);

  void BindGpuRequest(ui::mojom::GpuRequest request);

  ui::gpu_host::GpuHost* gpu_host_;
  discardable_memory::DiscardableSharedMemoryManager*
      discardable_shared_memory_manager_;

  DISALLOW_COPY_AND_ASSIGN(AshGpuInterfaceProvider);
};

}  // namespace ash

#endif  // ASH_WS_ASH_GPU_INTERFACE_PROVIDER_H_
