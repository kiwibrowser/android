// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_NETWORK_DELEGATE_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_NETWORK_DELEGATE_H_

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace ios_web_view {

// WebView implementation of NetworkDelegate.
class WebViewNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  WebViewNetworkDelegate();
  ~WebViewNetworkDelegate() override;

 private:
  // net::NetworkDelegate implementation.
  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewNetworkDelegate);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_NETWORK_DELEGATE_H_
