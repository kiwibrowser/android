// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/failing_url_request_interceptor.h"

#include "net/base/net_errors.h"
#include "net/url_request/url_request_error_job.h"

FailingURLRequestInterceptor::FailingURLRequestInterceptor() {}

FailingURLRequestInterceptor::~FailingURLRequestInterceptor() {}

net::URLRequestJob* FailingURLRequestInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return new net::URLRequestErrorJob(request, network_delegate,
                                     net::ERR_NOT_IMPLEMENTED);
}
