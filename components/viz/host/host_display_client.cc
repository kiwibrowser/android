// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_display_client.h"

#if defined(OS_MACOSX)
#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"
#endif

#if defined(OS_WIN)
#include <windows.h>

#include "components/viz/common/display/use_layered_window.h"
#include "components/viz/host/layered_window_updater_impl.h"
#include "ui/base/win/internal_constants.h"
#endif

namespace viz {

HostDisplayClient::HostDisplayClient(gfx::AcceleratedWidget widget)
    : binding_(this) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  widget_ = widget;
#endif
}

HostDisplayClient::~HostDisplayClient() = default;

mojom::DisplayClientPtr HostDisplayClient::GetBoundPtr(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  mojom::DisplayClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr), task_runner);
  return ptr;
}

void HostDisplayClient::DidSwapAfterSnapshotRequestReceived(
    const std::vector<ui::LatencyInfo>& latency_info) {}

#if defined(OS_MACOSX)
void HostDisplayClient::OnDisplayReceivedCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  ui::CALayerFrameSink* ca_layer_frame_sink =
      ui::CALayerFrameSink::FromAcceleratedWidget(widget_);
  if (ca_layer_frame_sink)
    ca_layer_frame_sink->UpdateCALayerTree(ca_layer_params);
  else
    DLOG(WARNING) << "Received frame for non-existent widget.";
}
#endif

#if defined(OS_WIN)
void HostDisplayClient::CreateLayeredWindowUpdater(
    mojom::LayeredWindowUpdaterRequest request) {
  if (!NeedsToUseLayerWindow(widget_)) {
    DLOG(ERROR) << "HWND shouldn't be using a layered window";
    return;
  }

  layered_window_updater_ =
      std::make_unique<LayeredWindowUpdaterImpl>(widget_, std::move(request));
}
#endif

}  // namespace viz
