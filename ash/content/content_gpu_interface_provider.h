// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_CONTENT_GPU_INTERFACE_PROVIDER_H_
#define ASH_CONTENT_CONTENT_GPU_INTERFACE_PROVIDER_H_

#include <memory>
#include <vector>

#include "ash/content/ash_with_content_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "services/ui/ws2/gpu_interface_provider.h"

namespace ash {

// An implementation of GpuInterfaceProvider that forwards to the Gpu
// implementation in content.
class ASH_WITH_CONTENT_EXPORT ContentGpuInterfaceProvider
    : public ui::ws2::GpuInterfaceProvider {
 public:
  ContentGpuInterfaceProvider();
  ~ContentGpuInterfaceProvider() override;

  // ui::ws2::GpuInterfaceProvider:
  void RegisterGpuInterfaces(
      service_manager::BinderRegistry* registry) override;

 private:
  class InterfaceBinderImpl;

  scoped_refptr<InterfaceBinderImpl> interface_binder_impl_;

  DISALLOW_COPY_AND_ASSIGN(ContentGpuInterfaceProvider);
};

}  // namespace ash

#endif  // ASH_CONTENT_CONTENT_GPU_INTERFACE_PROVIDER_H_
