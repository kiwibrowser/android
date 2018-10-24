// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_network_delegate.h"

#include "net/base/net_errors.h"

namespace ios_web_view {

WebViewNetworkDelegate::WebViewNetworkDelegate() {}

WebViewNetworkDelegate::~WebViewNetworkDelegate() = default;

bool WebViewNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}

}  // namespace ios_web_view
