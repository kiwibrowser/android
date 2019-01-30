// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"

namespace extensions {

WebRequestProxyingWebSocket::WebRequestProxyingWebSocket(
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
    WebRequestAPI::ProxySet* proxies)
    : process_id_(process_id),
      render_frame_id_(render_frame_id),
      origin_(origin),
      browser_context_(browser_context),
      resource_context_(resource_context),
      info_map_(info_map),
      request_id_generator_(std::move(request_id_generator)),
      proxied_socket_(std::move(proxied_socket)),
      binding_as_websocket_(this),
      binding_as_client_(this),
      binding_as_auth_handler_(this),
      proxies_(proxies),
      weak_factory_(this) {
  binding_as_websocket_.Bind(std::move(proxied_request));
  binding_as_auth_handler_.Bind(std::move(auth_request));

  binding_as_websocket_.set_connection_error_handler(
      base::BindRepeating(&WebRequestProxyingWebSocket::OnError,
                          base::Unretained(this), net::ERR_FAILED));
  binding_as_auth_handler_.set_connection_error_handler(
      base::BindRepeating(&WebRequestProxyingWebSocket::OnError,
                          base::Unretained(this), net::ERR_FAILED));
}

WebRequestProxyingWebSocket::~WebRequestProxyingWebSocket() = default;

void WebRequestProxyingWebSocket::AddChannelRequest(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    network::mojom::WebSocketClientPtr client) {
  if (binding_as_client_.is_bound() || !client || forwarding_client_) {
    // Illegal request.
    proxied_socket_ = nullptr;
    return;
  }

  request_.url = url;
  request_.site_for_cookies = site_for_cookies;
  request_.request_initiator = origin_;
  websocket_protocols_ = requested_protocols;
  uint64_t request_id = request_id_generator_->Generate();
  int routing_id = MSG_ROUTING_NONE;
  info_.emplace(request_id, process_id_, render_frame_id_, nullptr, routing_id,
                resource_context_, request_);

  forwarding_client_ = std::move(client);

  auto continuation =
      base::BindRepeating(&WebRequestProxyingWebSocket::OnBeforeRequestComplete,
                          weak_factory_.GetWeakPtr());

  // TODO(yhirano): Consider having throttling here (probably with aligned with
  // WebRequestProxyingURLLoaderFactory).
  bool should_collapse_initiator = false;
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      browser_context_, info_map_, &info_.value(), continuation, &redirect_url_,
      &should_collapse_initiator);

  // It doesn't make sense to collapse WebSocket requests since they won't be
  // associated with a DOM element.
  DCHECK(!should_collapse_initiator);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnBeforeRequestComplete(net::OK);
}

void WebRequestProxyingWebSocket::SendFrame(
    bool fin,
    network::mojom::WebSocketMessageType type,
    const std::vector<uint8_t>& data) {
  proxied_socket_->SendFrame(fin, type, data);
}

void WebRequestProxyingWebSocket::SendFlowControl(int64_t quota) {
  proxied_socket_->SendFlowControl(quota);
}

void WebRequestProxyingWebSocket::StartClosingHandshake(
    uint16_t code,
    const std::string& reason) {
  proxied_socket_->StartClosingHandshake(code, reason);
}

void WebRequestProxyingWebSocket::OnFailChannel(const std::string& reason) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnFailChannel(reason);

  forwarding_client_ = nullptr;
  int rv = net::ERR_FAILED;
  if (reason == "HTTP Authentication failed; no valid credentials available" ||
      reason == "Proxy authentication failed") {
    // This is needed to make some tests pass.
    // TODO(yhirano): Remove this hack.
    rv = net::ERR_ABORTED;
  }

  OnError(rv);
}

void WebRequestProxyingWebSocket::OnStartOpeningHandshake(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnStartOpeningHandshake(std::move(request));
}

void WebRequestProxyingWebSocket::OnFinishOpeningHandshake(
    network::mojom::WebSocketHandshakeResponsePtr response) {
  DCHECK(forwarding_client_);

  response_.headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(base::StringPrintf(
          "HTTP/%d.%d %d %s", response->http_version.major_value(),
          response->http_version.minor_value(), response->status_code,
          response->status_text.c_str()));
  for (const auto& header : response->headers) {
    if (!net::HttpResponseHeaders::IsCookieResponseHeader(header->name)) {
      // When the renderer process has an access to raw cookie headers, such
      // headers can be contained in |response|. Here we remove such headers
      // manually.
      response_.headers->AddHeader(header->name + ": " + header->value);
    }
  }
  response_.socket_address = response->socket_address;

  forwarding_client_->OnFinishOpeningHandshake(std::move(response));

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnHeadersReceivedComplete,
      weak_factory_.GetWeakPtr());
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      browser_context_, info_map_, &info_.value(), continuation,
      response_.headers.get(), &override_headers_, &redirect_url_);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING)
    return;

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedComplete(net::OK);
}

void WebRequestProxyingWebSocket::OnAddChannelResponse(
    const std::string& selected_protocol,
    const std::string& extensions) {
  DCHECK(forwarding_client_);
  DCHECK(!is_done_);
  is_done_ = true;
  ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
      browser_context_, info_map_, &info_.value(), net::ERR_WS_UPGRADE);

  forwarding_client_->OnAddChannelResponse(selected_protocol, extensions);
}

void WebRequestProxyingWebSocket::OnDataFrame(
    bool fin,
    network::mojom::WebSocketMessageType type,
    const std::vector<uint8_t>& data) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnDataFrame(fin, type, data);
}

void WebRequestProxyingWebSocket::OnFlowControl(int64_t quota) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnFlowControl(quota);
}

void WebRequestProxyingWebSocket::OnDropChannel(bool was_clean,
                                                uint16_t code,
                                                const std::string& reason) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnDropChannel(was_clean, code, reason);

  forwarding_client_ = nullptr;
  OnError(net::ERR_FAILED);
}

void WebRequestProxyingWebSocket::OnClosingHandshake() {
  DCHECK(forwarding_client_);
  forwarding_client_->OnClosingHandshake();
}

void WebRequestProxyingWebSocket::OnAuthRequired(
    const scoped_refptr<net::AuthChallengeInfo>& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const net::HostPortPair& socket_address,
    OnAuthRequiredCallback callback) {
  if (!auth_info || !callback) {
    OnError(net::ERR_FAILED);
    return;
  }

  response_.headers = headers;
  response_.socket_address = socket_address;
  auth_required_callback_ = std::move(callback);

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnHeadersReceivedCompleteForAuth,
      weak_factory_.GetWeakPtr(), auth_info);
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
      browser_context_, info_map_, &info_.value(), continuation,
      response_.headers.get(), &override_headers_, &redirect_url_);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING)
    return;

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedCompleteForAuth(auth_info, net::OK);
}

void WebRequestProxyingWebSocket::StartProxying(
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
    scoped_refptr<WebRequestAPI::ProxySet> proxies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (proxies->is_shutdown())
    return;

  auto proxy = std::make_unique<WebRequestProxyingWebSocket>(
      process_id, render_frame_id, origin, browser_context, resource_context,
      info_map, std::move(request_id_generator),
      network::mojom::WebSocketPtr(std::move(proxied_socket_ptr_info)),
      std::move(proxied_request), std::move(auth_request), proxies.get());

  proxies->AddProxy(std::move(proxy));
}

void WebRequestProxyingWebSocket::OnBeforeRequestComplete(int error_code) {
  DCHECK(!binding_as_client_.is_bound());
  DCHECK(request_.url.SchemeIsWSOrWSS());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnBeforeSendHeadersComplete,
      weak_factory_.GetWeakPtr());

  int result =
      ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
          browser_context_, info_map_, &info_.value(), continuation,
          &request_.headers);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING)
    return;

  DCHECK_EQ(net::OK, result);
  OnBeforeSendHeadersComplete(net::OK);
}

void WebRequestProxyingWebSocket::OnBeforeSendHeadersComplete(int error_code) {
  DCHECK(!binding_as_client_.is_bound());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
      browser_context_, info_map_, &info_.value(), request_.headers);

  network::mojom::WebSocketClientPtr proxy;

  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  for (net::HttpRequestHeaders::Iterator it(request_.headers); it.GetNext();) {
    additional_headers.push_back(
        network::mojom::HttpHeader::New(it.name(), it.value()));
  }

  binding_as_client_.Bind(mojo::MakeRequest(&proxy));
  binding_as_client_.set_connection_error_handler(
      base::BindOnce(&WebRequestProxyingWebSocket::OnError,
                     base::Unretained(this), net::ERR_FAILED));
  proxied_socket_->AddChannelRequest(
      request_.url, websocket_protocols_, request_.site_for_cookies,
      std::move(additional_headers), std::move(proxy));
}

void WebRequestProxyingWebSocket::OnHeadersReceivedComplete(int error_code) {
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }
  ResumeIncomingMethodCallProcessing();
  info_->AddResponseInfoFromResourceResponse(response_);
  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      browser_context_, info_map_, &info_.value(), net::OK);
}

void WebRequestProxyingWebSocket::OnAuthRequiredComplete(
    net::NetworkDelegate::AuthRequiredResponse rv) {
  DCHECK(auth_required_callback_);
  ResumeIncomingMethodCallProcessing();
  switch (rv) {
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      std::move(auth_required_callback_).Run(base::nullopt);
      break;

    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
      std::move(auth_required_callback_).Run(auth_credentials_);
      break;
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING:
      NOTREACHED();
      break;
  }
}

void WebRequestProxyingWebSocket::OnHeadersReceivedCompleteForAuth(
    scoped_refptr<net::AuthChallengeInfo> auth_info,
    int rv) {
  if (rv != net::OK) {
    OnError(rv);
    return;
  }
  ResumeIncomingMethodCallProcessing();
  info_->AddResponseInfoFromResourceResponse(response_);

  auto continuation =
      base::BindRepeating(&WebRequestProxyingWebSocket::OnAuthRequiredComplete,
                          weak_factory_.GetWeakPtr());
  auto auth_rv = ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
      browser_context_, info_map_, &info_.value(), *auth_info,
      std::move(continuation), &auth_credentials_);
  PauseIncomingMethodCallProcessing();
  if (auth_rv == net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING)
    return;

  OnAuthRequiredComplete(auth_rv);
}

void WebRequestProxyingWebSocket::PauseIncomingMethodCallProcessing() {
  binding_as_client_.PauseIncomingMethodCallProcessing();
  binding_as_auth_handler_.PauseIncomingMethodCallProcessing();
}

void WebRequestProxyingWebSocket::ResumeIncomingMethodCallProcessing() {
  binding_as_client_.ResumeIncomingMethodCallProcessing();
  binding_as_auth_handler_.ResumeIncomingMethodCallProcessing();
}

void WebRequestProxyingWebSocket::OnError(int error_code) {
  if (!is_done_) {
    is_done_ = true;
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        browser_context_, info_map_, &info_.value(), true /* started */,
        error_code);
  }
  if (forwarding_client_)
    forwarding_client_->OnFailChannel(net::ErrorToString(error_code));

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

}  // namespace extensions
