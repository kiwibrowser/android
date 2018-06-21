// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_NETWORK_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_NETWORK_DELEGATE_H_

#include "base/macros.h"

#include "base/synchronization/lock.h"
#include "headless/public/headless_browser_context.h"
#include "net/base/network_delegate_impl.h"

namespace headless {
class HeadlessBrowserContextImpl;

// We use the HeadlessNetworkDelegate to remove DevTools request headers before
// requests are actually fetched and for reporting failed network requests.
class HeadlessNetworkDelegate : public net::NetworkDelegateImpl,
                                public HeadlessBrowserContext::Observer {
 public:
  explicit HeadlessNetworkDelegate(
      HeadlessBrowserContextImpl* headless_browser_context);
  ~HeadlessNetworkDelegate() override;

 private:
  // net::NetworkDelegateImpl implementation:
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;

  void OnCompleted(net::URLRequest* request,
                   bool started,
                   int net_error) override;

  bool OnCanAccessFile(const net::URLRequest& request,
                       const base::FilePath& original_path,
                       const base::FilePath& absolute_path) const override;

  // HeadlessBrowserContext::Observer implementation:
  void OnHeadlessBrowserContextDestruct() override;

  base::Lock lock_;  // Protects |headless_browser_context_|.
  HeadlessBrowserContextImpl* headless_browser_context_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(HeadlessNetworkDelegate);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_NETWORK_DELEGATE_H_
