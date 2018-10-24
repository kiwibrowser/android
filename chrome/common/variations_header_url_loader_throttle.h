// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_VARIATIONS_HEADER_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_VARIATIONS_HEADER_URL_LOADER_THROTTLE_H_

#include "content/public/common/url_loader_throttle.h"

// This class adds the variations header for requests to Google, and ensures
// they're removed if a redirect to a non-Google URL occurs.
class VariationsHeaderURLLoaderThrottle
    : public content::URLLoaderThrottle,
      public base::SupportsWeakPtr<VariationsHeaderURLLoaderThrottle> {
 public:
  VariationsHeaderURLLoaderThrottle(bool is_off_the_record, bool is_signed_in);
  ~VariationsHeaderURLLoaderThrottle() override;

 private:
  // content::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      const net::RedirectInfo& redirect_info,
      const network::ResourceResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers) override;

  bool is_off_the_record_;
  bool is_signed_in_;
};

#endif  // CHROME_COMMON_VARIATIONS_HEADER_URL_LOADER_THROTTLE_H_
