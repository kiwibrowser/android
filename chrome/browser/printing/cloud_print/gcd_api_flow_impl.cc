// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/gcd_api_flow_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/printing/cloud_print/gcd_constants.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "components/cloud_devices/common/cloud_devices_urls.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

using net::DefineNetworkTrafficAnnotation;

namespace cloud_print {

namespace {

const char kCloudPrintOAuthHeaderFormat[] = "Authorization: Bearer %s";

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotation(
    GCDApiFlow::Request::NetworkTrafficAnnotation type) {
  if (type == CloudPrintApiFlowRequest::TYPE_PRIVET_REGISTER) {
    return DefineNetworkTrafficAnnotation("cloud_print_privet_register", R"(
        semantics {
          sender: "Cloud Print"
          description:
            "Registers a locally discovered Privet printer with a Cloud Print "
            "Server."
          trigger:
            "Users can select Privet printers on chrome://devices/ and "
            "register them."
          data:
            "Token id for a printer retrieved from a previous request to a "
            "Cloud Print Server."
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "User triggered requests cannot be disabled."
          policy_exception_justification: "Not implemented, it's good to do so."
        })");
  } else {
    DCHECK_EQ(CloudPrintApiFlowRequest::TYPE_SEARCH, type);
    return DefineNetworkTrafficAnnotation("cloud_print_search", R"(
        semantics {
          sender: "Cloud Print"
          description:
            "Queries a Cloud Print Server for the list of printers."
          trigger:
            "chrome://devices/ fetches the list when the user logs in, "
            "re-enable the Cloud Print service, or manually requests a printer "
            "list refresh."
          data: "None"
          destination: OTHER
        }
        policy {
          cookies_allowed: NO
          setting: "User triggered requests cannot be disabled."
          policy_exception_justification: "Not implemented, it's good to do so."
        })");
  }
}

}  // namespace

GCDApiFlowImpl::GCDApiFlowImpl(net::URLRequestContextGetter* request_context,
                               identity::IdentityManager* identity_manager)
    : request_context_(request_context), identity_manager_(identity_manager) {}

GCDApiFlowImpl::~GCDApiFlowImpl() {
}

void GCDApiFlowImpl::Start(std::unique_ptr<Request> request) {
  request_ = std::move(request);
  OAuth2TokenService::ScopeSet oauth_scopes;
  oauth_scopes.insert(request_->GetOAuthScope());
  DCHECK(identity_manager_);
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForPrimaryAccount(
      "cloud_print", oauth_scopes,
      base::BindOnce(&GCDApiFlowImpl::OnAccessTokenFetchComplete,
                     base::Unretained(this)),
      identity::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void GCDApiFlowImpl::OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                                std::string access_token) {
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    request_->OnGCDApiFlowError(ERROR_TOKEN);
    return;
  }

  CreateRequest();

  std::string authorization_header =
      base::StringPrintf(kCloudPrintOAuthHeaderFormat, access_token.c_str());

  url_fetcher_->AddExtraRequestHeader(authorization_header);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->Start();
}

void GCDApiFlowImpl::CreateRequest() {
  url_fetcher_ = net::URLFetcher::Create(
      request_->GetURL(), net::URLFetcher::GET, this,
      GetNetworkTrafficAnnotation(request_->GetNetworkTrafficAnnotationType()));
  url_fetcher_->SetRequestContext(request_context_.get());

  std::vector<std::string> extra_headers = request_->GetExtraRequestHeaders();
  for (const std::string& header : extra_headers)
    url_fetcher_->AddExtraRequestHeader(header);

  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(), data_use_measurement::DataUseUserData::CLOUD_PRINT);
}

void GCDApiFlowImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(noamsml): Error logging.

  // TODO(noamsml): Extract this and PrivetURLFetcher::OnURLFetchComplete into
  // one helper method.
  std::string response_str;

  if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
      !source->GetResponseAsString(&response_str)) {
    request_->OnGCDApiFlowError(ERROR_NETWORK);
    return;
  }

  if (source->GetResponseCode() != net::HTTP_OK) {
    request_->OnGCDApiFlowError(ERROR_HTTP_CODE);
    return;
  }

  base::JSONReader reader;
  std::unique_ptr<const base::Value> value(reader.Read(response_str));
  const base::DictionaryValue* dictionary_value = NULL;

  if (!value || !value->GetAsDictionary(&dictionary_value)) {
    request_->OnGCDApiFlowError(ERROR_MALFORMED_RESPONSE);
    return;
  }

  request_->OnGCDApiFlowComplete(*dictionary_value);
}

}  // namespace cloud_print
