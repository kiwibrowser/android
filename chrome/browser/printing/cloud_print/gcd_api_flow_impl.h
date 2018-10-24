// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_IMPL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}
class GoogleServiceAuthError;

namespace cloud_print {

class GCDApiFlowImpl : public GCDApiFlow, public net::URLFetcherDelegate {
 public:
  // Create an OAuth2-based confirmation.
  GCDApiFlowImpl(net::URLRequestContextGetter* request_context,
                 identity::IdentityManager* identity_manager);
  ~GCDApiFlowImpl() override;

  // GCDApiFlow implementation:
  void Start(std::unique_ptr<Request> request) override;

  // net::URLFetcherDelegate implementation:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  std::string access_token);

 private:
  void CreateRequest();

  std::unique_ptr<net::URLFetcher> url_fetcher_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  identity::IdentityManager* identity_manager_;
  std::unique_ptr<Request> request_;

  DISALLOW_COPY_AND_ASSIGN(GCDApiFlowImpl);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_IMPL_H_
