// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_JSON_REQUEST_H_
#define COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_JSON_REQUEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/ntp_snippets/status.h"
#include "net/http/http_request_headers.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace ntp_snippets {

namespace internal {

// A single request to subscribe for breaking news via GCM. The Request has to
// stay alive in order to be successfully completed.
class SubscriptionJsonRequest {
 public:
  // A client can expect a message in the status only, if there was any error
  // during the subscription. In successful cases, it will be an empty string.
  using CompletedCallback = base::OnceCallback<void(const Status& status)>;

  // Builds non-authenticated and authenticated SubscriptionJsonRequests.
  class Builder {
   public:
    Builder();
    Builder(Builder&&);
    ~Builder();

    // Builds a Request object that contains all data to fetch new snippets.
    std::unique_ptr<SubscriptionJsonRequest> Build() const;

    Builder& SetToken(const std::string& token);
    Builder& SetUrl(const GURL& url);
    Builder& SetUrlLoaderFactory(
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
    Builder& SetAuthenticationHeader(const std::string& auth_header);

    // The application language represented as an IETF language tag, defined in
    // BCP 47, e.g. "de", "de-AT".
    Builder& SetLocale(const std::string& locale);

    // The device country represented as lowercase ISO 3166-1 alpha-2, e.g.
    // "us", "in".
    // TODO(vitaliii): Use CLDR. Currently this is not possible, because the
    // variations permanent country is not provided in CLDR.
    Builder& SetCountryCode(const std::string& country_code);

   private:
    std::string BuildBody() const;
    std::unique_ptr<network::SimpleURLLoader> BuildURLLoader(
        const std::string& body) const;

    // GCM subscription token obtained from GCM driver (instanceID::getToken()).
    std::string token_;

    std::string locale_;
    std::string country_code_;

    GURL url_;
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    std::string auth_header_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  ~SubscriptionJsonRequest();

  // Starts an async request. The callback is invoked when the request succeeds
  // or fails. The callback is not called if the request is destroyed.
  void Start(CompletedCallback callback);

 private:
  friend class Builder;
  SubscriptionJsonRequest();
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // The loader for subscribing.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // The loader factory for subscribing.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The callback to notify when SimpleURLLoader finished and results are
  // available. When the request is finished/aborted/destroyed, it's called in
  // the dtor!
  CompletedCallback request_completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionJsonRequest);
};

}  // namespace internal

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_BREAKING_NEWS_SUBSCRIPTION_JSON_REQUEST_H_
