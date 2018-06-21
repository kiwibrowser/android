// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NAVIGATION_CLIENT_H_
#define CONTENT_RENDERER_NAVIGATION_CLIENT_H_

#include "content/common/navigation_client.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace content {

class RenderFrameImpl;

class NavigationClient : mojom::NavigationClient {
 public:
  explicit NavigationClient(RenderFrameImpl* render_frame);
  ~NavigationClient() override;

  // mojom::NavigationClient implementation:
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

  void Bind(mojom::NavigationClientAssociatedRequest request);

 private:
  // OnDroppedNavigation is bound from BeginNavigation till CommitNavigation.
  // During this period, it is called when the interface pipe is closed from the
  // browser side, leading to the ongoing navigation cancelation.
  void OnDroppedNavigation();
  void SetDisconnectionHandler();
  void ResetDisconnectionHandler();

  mojo::AssociatedBinding<mojom::NavigationClient> navigation_client_binding_;
  RenderFrameImpl* render_frame_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_NAVIGATION_CLIENT_H_
