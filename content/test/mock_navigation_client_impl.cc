// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_navigation_client_impl.h"

#include "content/public/browser/browser_thread.h"
#include "content/test/test_render_frame_host.h"

namespace content {

MockNavigationClientImpl::MockNavigationClientImpl(
    mojom::NavigationClientAssociatedRequest request)
    : navigation_client_binding_(this, std::move(request)) {}

MockNavigationClientImpl::~MockNavigationClientImpl() {}

void MockNavigationClientImpl::CommitNavigation(
    const network::ResourceResponseHead& head,
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders,
    base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
        subresource_overrides,
    mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
    const base::UnguessableToken& devtools_navigation_token) {}

void MockNavigationClientImpl::CommitFailedNavigation(
    const CommonNavigationParams& common_params,
    const RequestNavigationParams& request_params,
    bool has_stale_copy_in_cache,
    int error_code,
    const base::Optional<std::string>& error_page_content,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders) {}

}  // namespace content
