// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_frame_metadata_observer_impl.h"

#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace content {

RenderFrameMetadataObserverImpl::RenderFrameMetadataObserverImpl(
    mojom::RenderFrameMetadataObserverRequest request,
    mojom::RenderFrameMetadataObserverClientPtrInfo client_info)
    : request_(std::move(request)),
      client_info_(std::move(client_info)),
      render_frame_metadata_observer_binding_(this) {}

RenderFrameMetadataObserverImpl::~RenderFrameMetadataObserverImpl() {}

void RenderFrameMetadataObserverImpl::BindToCurrentThread() {
  DCHECK(request_.is_pending());
  render_frame_metadata_observer_binding_.Bind(std::move(request_));
  render_frame_metadata_observer_client_.Bind(std::move(client_info_));
}

void RenderFrameMetadataObserverImpl::OnRenderFrameSubmission(
    const cc::RenderFrameMetadata& render_frame_metadata,
    viz::CompositorFrameMetadata* compositor_frame_metadata) {
  // By default only report metadata changes for fields which have a low
  // frequency of change. However if there are changes in high frequency
  // fields these can be reported while testing is enabled.
  bool send_metadata = false;
  if (render_frame_metadata_observer_client_) {
    if (report_all_frame_submissions_for_testing_enabled_) {
      last_frame_token_ = compositor_frame_metadata->frame_token;
      compositor_frame_metadata->send_frame_token_to_embedder = true;
      render_frame_metadata_observer_client_->OnFrameSubmissionForTesting(
          last_frame_token_);
      send_metadata = !last_render_frame_metadata_ ||
                      *last_render_frame_metadata_ != render_frame_metadata;
    } else {
      send_metadata = !last_render_frame_metadata_ ||
                      cc::RenderFrameMetadata::HasAlwaysUpdateMetadataChanged(
                          *last_render_frame_metadata_, render_frame_metadata);
    }
  }

  // Allways cache the full metadata, so that it can correctly be sent upon
  // ReportAllFrameSubmissionsForTesting. This must only be done after we've
  // compared the two for changes.
  last_render_frame_metadata_ = render_frame_metadata;

  // If the metadata is different, updates all the observers; or the metadata is
  // generated for first time and same as the default value, update the default
  // value to all the observers.
  if (send_metadata && render_frame_metadata_observer_client_) {
    // Sending |root_scroll_offset| outside of tests would leave the browser
    // process with out of date information. It is an optional parameter
    // which we clear here.
    auto metadata_copy = render_frame_metadata;
    if (!report_all_frame_submissions_for_testing_enabled_)
      metadata_copy.root_scroll_offset = base::nullopt;

    last_frame_token_ = compositor_frame_metadata->frame_token;
    compositor_frame_metadata->send_frame_token_to_embedder = true;
    render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
        last_frame_token_, metadata_copy);
  }

  // Always cache the initial frame token, so that if a test connects later on
  // it can be notified of the initial state.
  if (!last_frame_token_) {
    last_frame_token_ = compositor_frame_metadata->frame_token;
    compositor_frame_metadata->send_frame_token_to_embedder = true;
  }
}

void RenderFrameMetadataObserverImpl::ReportAllFrameSubmissionsForTesting(
    bool enabled) {
  report_all_frame_submissions_for_testing_enabled_ = enabled;

  if (!enabled || !last_frame_token_)
    return;

  // When enabled for testing send the cached metadata.
  DCHECK(render_frame_metadata_observer_client_);
  DCHECK(last_render_frame_metadata_.has_value());
  render_frame_metadata_observer_client_->OnRenderFrameMetadataChanged(
      last_frame_token_, *last_render_frame_metadata_);
}

}  // namespace content
