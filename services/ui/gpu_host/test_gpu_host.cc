// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu_host/test_gpu_host.h"

namespace ui {
namespace gpu_host {

TestGpuHost::TestGpuHost() = default;

TestGpuHost::~TestGpuHost() = default;

void TestGpuHost::CreateFrameSinkManager(
    viz::mojom::FrameSinkManagerParamsPtr params) {
  frame_sink_manager_ = std::make_unique<viz::TestFrameSinkManagerImpl>();
  viz::mojom::FrameSinkManagerClientPtr client(
      std::move(params->frame_sink_manager_client));
  frame_sink_manager_->BindRequest(std::move(params->frame_sink_manager),
                                   std::move(client));
}

}  // namespace gpu_host
}  // namespace ui
