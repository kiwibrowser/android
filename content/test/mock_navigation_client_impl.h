// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_NAVIGATION_CLIENT_IMPL_H_
#define CONTENT_TEST_MOCK_NAVIGATION_CLIENT_IMPL_H_

#include "content/common/frame.mojom.h"
#include "content/common/navigation_client.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace content {

class MockNavigationClientImpl : public mojom::NavigationClient {
 public:
  MockNavigationClientImpl(mojom::NavigationClientAssociatedRequest request);
  ~MockNavigationClientImpl() override;

  // mojom::NavigationClient implementation
  void CommitNavigation(
      const network::ResourceResponseHead& head,
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders,
      base::Optional<std::vector<::content::mojom::TransferrableURLLoaderPtr>>
          subresource_overrides,
      mojom::ControllerServiceWorkerInfoPtr controller_service_worker_info,
      const base::UnguessableToken& devtools_navigation_token) override;
  void CommitFailedNavigation(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      bool has_stale_copy_in_cache,
      int error_code,
      const base::Optional<std::string>& error_page_content,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loaders) override;

 private:
  mojo::AssociatedBinding<mojom::NavigationClient> navigation_client_binding_;
};

}  // namespace content

#endif
