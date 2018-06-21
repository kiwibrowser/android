// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_client.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/task_type.h"

namespace content {

NavigationClient::NavigationClient(RenderFrameImpl* render_frame)
    : navigation_client_binding_(this), render_frame_(render_frame) {}

NavigationClient::~NavigationClient() {}

void NavigationClient::CommitNavigation(
    const network::ResourceResponseHead& head,
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders,
    base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    const base::UnguessableToken& devtools_navigation_token) {
  // TODO(ahemery): The reset should be done when the navigation did commit
  // (meaning at a later stage). This is not currently possible because of
  // race conditions leading to the early deletion of NavigationRequest would
  // unexpectedly abort the ongoing navigation. Remove when the races are fixed.
  ResetDisconnectionHandler();
  render_frame_->CommitNavigation(
      head, common_params, request_params,
      std::move(url_loader_client_endpoints), std::move(subresource_loaders),
      std::move(subresource_overrides),
      std::move(controller_service_worker_info), devtools_navigation_token,
      mojom::FrameNavigationControl::CommitNavigationCallback());
}

void NavigationClient::CommitFailedNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool has_stale_copy_in_cache,
    int error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders) {
  ResetDisconnectionHandler();
  render_frame_->CommitFailedNavigation(
      common_params, request_params, has_stale_copy_in_cache, error_code,
      error_page_content, std::move(subresource_loaders),
      mojom::FrameNavigationControl::CommitFailedNavigationCallback());
}

void NavigationClient::Bind(mojom::NavigationClientAssociatedRequest request) {
  navigation_client_binding_.Bind(
      std::move(request),
      render_frame_->GetTaskRunner(blink::TaskType::kInternalIPC));
  SetDisconnectionHandler();
}

void NavigationClient::SetDisconnectionHandler() {
  navigation_client_binding_.set_connection_error_handler(base::BindOnce(
      &NavigationClient::OnDroppedNavigation, base::Unretained(this)));
}

void NavigationClient::ResetDisconnectionHandler() {
  navigation_client_binding_.set_connection_error_handler(base::DoNothing());
}

void NavigationClient::OnDroppedNavigation() {
  render_frame_->OnDroppedNavigation();
}

}  // namespace content
