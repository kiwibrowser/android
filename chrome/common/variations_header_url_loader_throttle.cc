// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/variations_header_url_loader_throttle.h"

#include "components/variations/net/variations_http_headers.h"

VariationsHeaderURLLoaderThrottle::VariationsHeaderURLLoaderThrottle(
    bool is_off_the_record,
    bool is_signed_in)
    : is_off_the_record_(is_off_the_record), is_signed_in_(is_signed_in) {}

VariationsHeaderURLLoaderThrottle::~VariationsHeaderURLLoaderThrottle() {}

void VariationsHeaderURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  net::HttpRequestHeaders to_be_added_headers;
  variations::AppendVariationHeaders(
      request->url,
      is_off_the_record_ ? variations::InIncognito::kYes
                         : variations::InIncognito::kNo,
      is_signed_in_ ? variations::SignedIn::kYes : variations::SignedIn::kNo,
      &to_be_added_headers);

  request->headers.MergeFrom(to_be_added_headers);
}

void VariationsHeaderURLLoaderThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers) {
  if (!variations::internal::ShouldAppendVariationHeaders(
          redirect_info.new_url)) {
    const char kClientData[] = "X-Client-Data";
    to_be_removed_headers->push_back(kClientData);
  }
}
