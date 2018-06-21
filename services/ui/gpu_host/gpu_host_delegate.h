// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_HOST_GPU_HOST_DELEGATE_H_
#define SERVICES_UI_GPU_HOST_GPU_HOST_DELEGATE_H_

#include "base/memory/ref_counted.h"

namespace ui {
namespace gpu_host {

class GpuHostDelegate {
 public:
  virtual ~GpuHostDelegate() {}

  virtual void OnGpuServiceInitialized() = 0;
};

}  // namespace gpu_host
}  // namespace ui

#endif  // SERVICES_UI_GPU_HOST_GPU_HOST_DELEGATE_H_
