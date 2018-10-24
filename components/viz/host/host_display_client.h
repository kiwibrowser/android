// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_
#define COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/host/viz_host_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/viz/privileged/interfaces/compositing/display_private.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace viz {

class LayeredWindowUpdaterImpl;

// mojom::DisplayClient implementation that relays calls to platform specific
// functions.
class VIZ_HOST_EXPORT HostDisplayClient : public mojom::DisplayClient {
 public:
  explicit HostDisplayClient(gfx::AcceleratedWidget widget);
  ~HostDisplayClient() override;

  mojom::DisplayClientPtr GetBoundPtr(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  // mojom::DisplayClient implementation:
  void DidSwapAfterSnapshotRequestReceived(
      const std::vector<ui::LatencyInfo>& latency_info) override;

#if defined(OS_MACOSX)
  void OnDisplayReceivedCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
#endif

#if defined(OS_WIN)
  void CreateLayeredWindowUpdater(
      mojom::LayeredWindowUpdaterRequest request) override;
#endif

  mojo::Binding<mojom::DisplayClient> binding_;
#if defined(OS_MACOSX) || defined(OS_WIN)
  gfx::AcceleratedWidget widget_;
#endif

#if defined(OS_WIN)
  std::unique_ptr<LayeredWindowUpdaterImpl> layered_window_updater_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HostDisplayClient);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_DISPLAY_CLIENT_H_
