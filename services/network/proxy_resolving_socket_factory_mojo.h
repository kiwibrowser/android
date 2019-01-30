// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_FACTORY_MOJO_H_
#define SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_FACTORY_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"

namespace net {
class URLRequestContext;
}  // namespace net

namespace network {

class ProxyResolvingClientSocketFactory;

class COMPONENT_EXPORT(NETWORK_SERVICE) ProxyResolvingSocketFactoryMojo
    : public mojom::ProxyResolvingSocketFactory {
 public:
  ProxyResolvingSocketFactoryMojo(net::URLRequestContext* request_context);
  ~ProxyResolvingSocketFactoryMojo() override;

  // mojom::ProxyResolvingSocketFactory implementation.
  void CreateProxyResolvingSocket(
      const GURL& url,
      bool use_tls,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojom::ProxyResolvingSocketRequest request,
      CreateProxyResolvingSocketCallback callback) override;

 private:
  std::unique_ptr<ProxyResolvingClientSocketFactory> factory_impl_;
  mojo::StrongBindingSet<mojom::ProxyResolvingSocket>
      proxy_resolving_socket_bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResolvingSocketFactoryMojo);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_RESOLVING_SOCKET_FACTORY_MOJO_H_
