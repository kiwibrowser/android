// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_MOJO_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/proxy_resolving_client_socket.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

namespace network {

class SocketDataPump;

class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyResolvingSocketMojo
    : public mojom::ProxyResolvingSocket {
 public:
  ProxyResolvingSocketMojo(
      std::unique_ptr<ProxyResolvingClientSocket> socket,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);
  ~ProxyResolvingSocketMojo() override;
  void Connect(
      mojom::ProxyResolvingSocketFactory::CreateProxyResolvingSocketCallback
          callback);

  // mojom::ProxyResolvingSocket implementation.
  void GetPeerAddress(GetPeerAddressCallback callback) override;

 private:
  void OnConnectCompleted(int net_result);

  std::unique_ptr<ProxyResolvingClientSocket> socket_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  mojom::ProxyResolvingSocketFactory::CreateProxyResolvingSocketCallback
      connect_callback_;
  std::unique_ptr<SocketDataPump> socket_data_pump_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingSocketMojo);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_MOJO_H_
