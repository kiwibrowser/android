// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/hit_test_region_observer.h"

#include <algorithm>

#include "base/test/test_timeouts.h"
#include "components/viz/common/features.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/frame_connector_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

namespace {
class SurfaceHitTestReadyNotifier {
 public:
  explicit SurfaceHitTestReadyNotifier(RenderWidgetHostViewBase* target_view);
  ~SurfaceHitTestReadyNotifier() {}

  void WaitForSurfaceReady(RenderWidgetHostViewBase* root_container);

 private:
  bool ContainsSurfaceId(const viz::SurfaceId& container_surface_id);

  viz::SurfaceManager* surface_manager_;
  RenderWidgetHostViewBase* target_view_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceHitTestReadyNotifier);
};

SurfaceHitTestReadyNotifier::SurfaceHitTestReadyNotifier(
    RenderWidgetHostViewBase* target_view)
    : target_view_(target_view) {
  surface_manager_ = GetFrameSinkManager()->surface_manager();
}

void SurfaceHitTestReadyNotifier::WaitForSurfaceReady(
    RenderWidgetHostViewBase* root_view) {
  viz::SurfaceId root_surface_id = root_view->GetCurrentSurfaceId();
  while (!ContainsSurfaceId(root_surface_id)) {
    // TODO(kenrb): Need a better way to do this. Needs investigation on
    // whether we can add a callback through RenderWidgetHostViewBaseObserver
    // from OnSwapCompositorFrame and avoid this busy waiting. A callback on
    // every compositor frame might be generally undesirable for performance,
    // however.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

bool SurfaceHitTestReadyNotifier::ContainsSurfaceId(
    const viz::SurfaceId& container_surface_id) {
  if (!container_surface_id.is_valid())
    return false;

  viz::Surface* container_surface =
      surface_manager_->GetSurfaceForId(container_surface_id);
  if (!container_surface || !container_surface->active_referenced_surfaces())
    return false;

  for (const viz::SurfaceId& id :
       *container_surface->active_referenced_surfaces()) {
    if (id == target_view_->GetCurrentSurfaceId() || ContainsSurfaceId(id))
      return true;
  }
  return false;
}

// Waits until the cc::Surface associated with a guest/cross-process-iframe
// has been drawn for the first time. Once this method returns it should be
// safe to assume that events sent to the top-level RenderWidgetHostView can
// be expected to properly hit-test to this surface, if appropriate.
void WaitForGuestSurfaceReady(content::WebContents* guest_web_contents) {
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          guest_web_contents->GetRenderWidgetHostView());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      static_cast<content::WebContentsImpl*>(guest_web_contents)
          ->GetOuterWebContents()
          ->GetRenderWidgetHostView());

  SurfaceHitTestReadyNotifier notifier(child_view);
  notifier.WaitForSurfaceReady(root_view);
}

// To wait for frame submission see RenderFrameSubmissionObserver.
// Waits until the cc::Surface associated with a cross-process child frame
// has been drawn for the first time. Once this method returns it should be
// safe to assume that events sent to the top-level RenderWidgetHostView can
// be expected to properly hit-test to this surface, if appropriate.
void WaitForChildFrameSurfaceReady(content::RenderFrameHost* child_frame) {
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderFrameHostImpl*>(child_frame)
          ->GetRenderWidgetHost()
          ->GetView();
  if (!child_view || !child_view->IsRenderWidgetHostViewChildFrame())
    return;

  RenderWidgetHostViewBase* root_view =
      static_cast<CrossProcessFrameConnector*>(
          static_cast<RenderWidgetHostViewChildFrame*>(child_view)
              ->FrameConnectorForTesting())
          ->GetRootRenderWidgetHostViewForTesting();

  SurfaceHitTestReadyNotifier notifier(child_view);
  notifier.WaitForSurfaceReady(root_view);
}

}  // namespace

void WaitForHitTestDataOrChildSurfaceReady(RenderFrameHost* child_frame) {
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderFrameHostImpl*>(child_frame)
          ->GetRenderWidgetHost()
          ->GetView();

  if (features::IsVizHitTestingEnabled()) {
    HitTestRegionObserver observer(child_view->GetFrameSinkId());
    observer.WaitForHitTestData();
    return;
  }

  WaitForChildFrameSurfaceReady(child_frame);
}

void WaitForHitTestDataOrGuestSurfaceReady(WebContents* guest_web_contents) {
  DCHECK(static_cast<RenderWidgetHostViewBase*>(
             guest_web_contents->GetRenderWidgetHostView())
             ->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          guest_web_contents->GetRenderWidgetHostView());

  if (features::IsVizHitTestingEnabled()) {
    HitTestRegionObserver observer(child_view->GetFrameSinkId());
    observer.WaitForHitTestData();
    return;
  }

  WaitForGuestSurfaceReady(guest_web_contents);
}

HitTestRegionObserver::HitTestRegionObserver(
    const viz::FrameSinkId& frame_sink_id)
    : frame_sink_id_(frame_sink_id) {
  CHECK(frame_sink_id.is_valid());
  GetHostFrameSinkManager()->AddHitTestRegionObserver(this);
}

HitTestRegionObserver::~HitTestRegionObserver() {
  GetHostFrameSinkManager()->RemoveHitTestRegionObserver(this);
}

void HitTestRegionObserver::WaitForHitTestData() {
  for (auto& it : GetHostFrameSinkManager()->display_hit_test_query()) {
    if (it.second->ContainsFrameSinkId(frame_sink_id_)) {
      return;
    }
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void HitTestRegionObserver::OnAggregatedHitTestRegionListUpdated(
    const viz::FrameSinkId& frame_sink_id,
    const std::vector<viz::AggregatedHitTestRegion>& hit_test_data) {
  if (!run_loop_)
    return;

  for (auto& it : hit_test_data) {
    if (it.frame_sink_id == frame_sink_id_) {
      run_loop_->Quit();
      return;
    }
  }
}

}  // namespace content
