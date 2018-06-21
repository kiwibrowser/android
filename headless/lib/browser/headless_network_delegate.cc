// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_network_delegate.h"

#include "content/public/browser/resource_request_info.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

#include "net/url_request/url_request.h"

namespace headless {

HeadlessNetworkDelegate::HeadlessNetworkDelegate(
    HeadlessBrowserContextImpl* headless_browser_context)
    : headless_browser_context_(headless_browser_context) {
  base::AutoLock lock(lock_);
  if (headless_browser_context_)
    headless_browser_context_->AddObserver(this);
}

HeadlessNetworkDelegate::~HeadlessNetworkDelegate() {
  base::AutoLock lock(lock_);
  if (headless_browser_context_)
    headless_browser_context_->RemoveObserver(this);
}

int HeadlessNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    net::CompletionOnceCallback callback,
    GURL* new_url) {
  return net::OK;
}

void HeadlessNetworkDelegate::OnCompleted(net::URLRequest* request,
                                          bool started,
                                          int net_error) {
  base::AutoLock lock(lock_);
  if (!headless_browser_context_)
    return;

  const content::ResourceRequestInfo* resource_request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!resource_request_info)
    return;

  DevToolsStatus devtools_status = resource_request_info->GetDevToolsStatus();
  if (devtools_status != DevToolsStatus::kNotCanceled || net_error != net::OK) {
    headless_browser_context_->NotifyUrlRequestFailed(request, net_error,
                                                      devtools_status);
  }
}

bool HeadlessNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& original_path,
    const base::FilePath& absolute_path) const {
  return true;
}

void HeadlessNetworkDelegate::OnHeadlessBrowserContextDestruct() {
  base::AutoLock lock(lock_);
  headless_browser_context_ = nullptr;
}

}  // namespace headless
