// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_MEDIA_LOAD_DEFERRER_H_
#define CHROMECAST_RENDERER_CAST_MEDIA_LOAD_DEFERRER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "chromecast/common/mojom/media_load_deferrer.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace chromecast {

// Implements deferred media load for Chromecast devices, to prevent background
// applications from playing unwanted media. This functionality is based on
// Chrome prerender. Manages its own lifetime.
class CastMediaLoadDeferrer
    : public chromecast::shell::mojom::MediaLoadDeferrer,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CastMediaLoadDeferrer> {
 public:
  explicit CastMediaLoadDeferrer(content::RenderFrame* render_frame);

  // Runs |closure| if the page/frame is switched to foreground. Returns true if
  // the running of |closure| is deferred (not yet in foreground); false
  // |closure| can be run immediately.
  bool RunWhenInForeground(base::OnceClosure closure);

 private:
  ~CastMediaLoadDeferrer() override;

  // content::RenderFrameObserver implementation:
  void OnDestruct() override;

  // MediaLoadDeferrer implementation
  void UpdateMediaLoadStatus(bool blocked) override;

  void OnMediaLoadDeferrerAssociatedRequest(
      chromecast::shell::mojom::MediaLoadDeferrerAssociatedRequest request);

  bool render_frame_action_blocked_;

  std::vector<base::OnceClosure> pending_closures_;

  mojo::AssociatedBindingSet<chromecast::shell::mojom::MediaLoadDeferrer>
      bindings_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastMediaLoadDeferrer);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_MEDIA_LOAD_DEFERRER_H_
