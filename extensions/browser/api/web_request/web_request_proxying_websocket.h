// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_WEBSOCKET_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_WEBSOCKET_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/base/network_delegate.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// A WebRequestProxyingWebSocket proxies a WebSocket connection and dispatches
// WebRequest API events. This is used only when the network service is enabled.
class WebRequestProxyingWebSocket
    : public WebRequestAPI::Proxy,
      public network::mojom::WebSocket,
      public network::mojom::WebSocketClient,
      public network::mojom::AuthenticationHandler {
 public:
  WebRequestProxyingWebSocket(
      int process_id,
      int render_frame_id,
      const url::Origin& origin,
      content::BrowserContext* browser_context,
      content::ResourceContext* resource_context,
      InfoMap* info_map,
      scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator,
      network::mojom::WebSocketPtr proxied_socket,
      network::mojom::WebSocketRequest proxied_request,
      network::mojom::AuthenticationHandlerRequest auth_request,
      WebRequestAPI::ProxySet* proxies);
  ~WebRequestProxyingWebSocket() override;

  // mojom::WebSocket methods:
  void AddChannelRequest(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      const GURL& site_for_cookies,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      network::mojom::WebSocketClientPtr client) override;
  void SendFrame(bool fin,
                 network::mojom::WebSocketMessageType type,
                 const std::vector<uint8_t>& data) override;
  void SendFlowControl(int64_t quota) override;
  void StartClosingHandshake(uint16_t code, const std::string& reason) override;

  // mojom::WebSocketClient methods:
  void OnFailChannel(const std::string& reason) override;
  void OnStartOpeningHandshake(
      network::mojom::WebSocketHandshakeRequestPtr request) override;
  void OnFinishOpeningHandshake(
      network::mojom::WebSocketHandshakeResponsePtr response) override;
  void OnAddChannelResponse(const std::string& selected_protocol,
                            const std::string& extensions) override;
  void OnDataFrame(bool fin,
                   network::mojom::WebSocketMessageType type,
                   const std::vector<uint8_t>& data) override;
  void OnFlowControl(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const std::string& reason) override;
  void OnClosingHandshake() override;

  // mojom::AuthenticationHandler method:
  void OnAuthRequired(const scoped_refptr<net::AuthChallengeInfo>& auth_info,
                      const scoped_refptr<net::HttpResponseHeaders>& headers,
                      const net::HostPortPair& socket_address,
                      OnAuthRequiredCallback callback) override;

  static void StartProxying(
      int process_id,
      int render_frame_id,
      scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator,
      const url::Origin& origin,
      content::BrowserContext* browser_context,
      content::ResourceContext* resource_context,
      InfoMap* info_map,
      network::mojom::WebSocketPtrInfo proxied_socket_ptr_info,
      network::mojom::WebSocketRequest proxied_request,
      network::mojom::AuthenticationHandlerRequest auth_request,
      scoped_refptr<WebRequestAPI::ProxySet> proxies);

 private:
  void OnBeforeRequestComplete(int error_code);
  void OnBeforeSendHeadersComplete(int error_code);
  void OnHeadersReceivedComplete(int error_code);
  void OnAuthRequiredComplete(net::NetworkDelegate::AuthRequiredResponse rv);
  void OnHeadersReceivedCompleteForAuth(
      scoped_refptr<net::AuthChallengeInfo> auth_info,
      int rv);

  void PauseIncomingMethodCallProcessing();
  void ResumeIncomingMethodCallProcessing();
  void OnError(int result);

  const int process_id_;
  const int render_frame_id_;
  const url::Origin origin_;
  content::BrowserContext* const browser_context_;
  content::ResourceContext* const resource_context_;
  InfoMap* const info_map_;
  scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator_;
  network::mojom::WebSocketPtr proxied_socket_;
  network::mojom::WebSocketClientPtr forwarding_client_;
  mojo::Binding<network::mojom::WebSocket> binding_as_websocket_;
  mojo::Binding<network::mojom::WebSocketClient> binding_as_client_;
  mojo::Binding<network::mojom::AuthenticationHandler> binding_as_auth_handler_;

  network::ResourceRequest request_;
  network::ResourceResponseHead response_;
  net::AuthCredentials auth_credentials_;
  OnAuthRequiredCallback auth_required_callback_;
  scoped_refptr<net::HttpResponseHeaders> override_headers_;
  std::vector<std::string> websocket_protocols_;

  GURL redirect_url_;
  bool is_done_ = false;

  base::Optional<WebRequestInfo> info_;

  // Owns |this|.
  WebRequestAPI::ProxySet* const proxies_;

  base::WeakPtrFactory<WebRequestProxyingWebSocket> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WebRequestProxyingWebSocket);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_WEBSOCKET_H_
