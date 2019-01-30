// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_FAILING_URL_REQUEST_INTERCEPTOR_H_
#define CHROME_BROWSER_NET_FAILING_URL_REQUEST_INTERCEPTOR_H_

#include "net/url_request/url_request_interceptor.h"

// A URLRequestInterceptor that fails all network requests.
class FailingURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  FailingURLRequestInterceptor();
  ~FailingURLRequestInterceptor() override;

  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingURLRequestInterceptor);
};

#endif  // CHROME_BROWSER_NET_FAILING_URL_REQUEST_INTERCEPTOR_H_
