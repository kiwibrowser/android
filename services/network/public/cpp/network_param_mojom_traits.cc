// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_param_mojom_traits.h"

namespace mojo {

bool StructTraits<network::mojom::HttpVersionDataView, net::HttpVersion>::Read(
    network::mojom::HttpVersionDataView data,
    net::HttpVersion* out) {
  *out = net::HttpVersion(data.major_value(), data.minor_value());
  return true;
}

}  // namespace mojo
