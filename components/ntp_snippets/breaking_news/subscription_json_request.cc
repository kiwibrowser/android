// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_json_request.h"

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

using net::HttpRequestHeaders;

namespace ntp_snippets {

namespace internal {

SubscriptionJsonRequest::SubscriptionJsonRequest() = default;

SubscriptionJsonRequest::~SubscriptionJsonRequest() = default;

void SubscriptionJsonRequest::Start(CompletedCallback callback) {
  DCHECK(request_completed_callback_.is_null()) << "Request already running!";
  DCHECK(url_loader_factory_);
  request_completed_callback_ = std::move(callback);
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&SubscriptionJsonRequest::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void SubscriptionJsonRequest::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int net_error = simple_url_loader_->NetError();

  if (net_error != net::OK) {
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::TEMPORARY_ERROR,
                    base::StringPrintf("Network Error: %d", net_error)));
  } else if (!response_body) {
    int response_code = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::PERMANENT_ERROR,
                    base::StringPrintf("HTTP Error: %d", response_code)));
  } else {
    std::move(request_completed_callback_)
        .Run(Status(StatusCode::SUCCESS, std::string()));
  }
}

SubscriptionJsonRequest::Builder::Builder() = default;
SubscriptionJsonRequest::Builder::Builder(SubscriptionJsonRequest::Builder&&) =
    default;
SubscriptionJsonRequest::Builder::~Builder() = default;

std::unique_ptr<SubscriptionJsonRequest>
SubscriptionJsonRequest::Builder::Build() const {
  DCHECK(!url_.is_empty());
  DCHECK(url_loader_factory_);
  auto request = base::WrapUnique(new SubscriptionJsonRequest());

  std::string body = BuildBody();
  request->simple_url_loader_ = BuildURLLoader(body);
  request->url_loader_factory_ = std::move(url_loader_factory_);

  return request;
}

SubscriptionJsonRequest::Builder& SubscriptionJsonRequest::Builder::SetToken(
    const std::string& token) {
  token_ = token;
  return *this;
}

SubscriptionJsonRequest::Builder& SubscriptionJsonRequest::Builder::SetUrl(
    const GURL& url) {
  url_ = url;
  return *this;
}

SubscriptionJsonRequest::Builder&
SubscriptionJsonRequest::Builder::SetUrlLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = std::move(url_loader_factory);
  return *this;
}

SubscriptionJsonRequest::Builder&
SubscriptionJsonRequest::Builder::SetAuthenticationHeader(
    const std::string& auth_header) {
  auth_header_ = auth_header;
  return *this;
}

SubscriptionJsonRequest::Builder& SubscriptionJsonRequest::Builder::SetLocale(
    const std::string& locale) {
  locale_ = locale;
  return *this;
}

SubscriptionJsonRequest::Builder&
SubscriptionJsonRequest::Builder::SetCountryCode(
    const std::string& country_code) {
  country_code_ = country_code;
  return *this;
}

std::string SubscriptionJsonRequest::Builder::BuildBody() const {
  base::DictionaryValue request;
  request.SetString("token", token_);

  request.SetString("locale", locale_);
  request.SetString("country_code", country_code_);

  std::string request_json;
  bool success = base::JSONWriter::Write(request, &request_json);
  DCHECK(success);
  return request_json;
}

std::unique_ptr<network::SimpleURLLoader>
SubscriptionJsonRequest::Builder::BuildURLLoader(
    const std::string& body) const {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_subscription", R"(
        semantics {
          sender: "Subscribe for breaking news delivered via GCM push messages"
          description:
            "Chromium can receive breaking news via GCM push messages. "
            "This request suscribes the client to receiving them."
          trigger:
            "Subscription takes place only once per profile lifetime. "
          data:
            "The subscription token that identifies this Chromium profile."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings now"
          chrome_policy {
            NTPContentSuggestionsEnabled {
              policy_options {mode: MANDATORY}
              NTPContentSuggestionsEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->method = "POST";
  if (!auth_header_.empty()) {
    resource_request->headers.SetHeader(HttpRequestHeaders::kAuthorization,
                                        auth_header_);
  }
  // Add X-Client-Data header with experiment IDs from field trials.
  // Note: It's OK to pass SignedIn::kNo if it's unknown, as it does not affect
  // transmission of experiments coming from the variations server.
  variations::AppendVariationHeaders(url_, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo,
                                     &resource_request->headers);

  // Log the request for debugging network issues.
  DVLOG(1) << "Building a subscription request to " << url_ << ":\n"
           << resource_request->headers.ToString() << "\n"
           << body;

  // TODO(https://crbug.com/808498): Re-add data use measurement once
  // SimpleURLLoader supports it.
  // ID=data_use_measurement::DataUseUserData::NTP_SNIPPETS_SUGGESTIONS
  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  loader->AttachStringForUpload(body, "application/json; charset=UTF-8");
  loader->SetRetryOptions(1, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  return loader;
}

}  // namespace internal

}  // namespace ntp_snippets
