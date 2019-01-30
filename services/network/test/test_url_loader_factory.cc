// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_url_loader_factory.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace network {

TestURLLoaderFactory::PendingRequest::PendingRequest() = default;
TestURLLoaderFactory::PendingRequest::~PendingRequest() = default;

TestURLLoaderFactory::PendingRequest::PendingRequest(PendingRequest&& other) =
    default;
TestURLLoaderFactory::PendingRequest& TestURLLoaderFactory::PendingRequest::
operator=(PendingRequest&& other) = default;

TestURLLoaderFactory::Response::Response() = default;
TestURLLoaderFactory::Response::~Response() = default;
TestURLLoaderFactory::Response::Response(const Response&) = default;

TestURLLoaderFactory::TestURLLoaderFactory() {}

TestURLLoaderFactory::~TestURLLoaderFactory() {}

void TestURLLoaderFactory::AddResponse(const GURL& url,
                                       const ResourceResponseHead& head,
                                       const std::string& content,
                                       const URLLoaderCompletionStatus& status,
                                       const Redirects& redirects) {
  Response response;
  response.url = url;
  response.redirects = redirects;
  response.head = head;
  response.content = content;
  response.status = status;
  responses_[url] = response;

  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    if (CreateLoaderAndStartInternal(it->url, it->client.get())) {
      it = pending_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

void TestURLLoaderFactory::AddResponse(const std::string& url,
                                       const std::string& content,
                                       net::HttpStatusCode http_status) {
  ResourceResponseHead head;
  std::string headers(base::StringPrintf(
      "HTTP/1.1 %d %s\nContent-type: text/html\n\n",
      static_cast<int>(http_status), GetHttpReasonPhrase(http_status)));
  head.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
  head.mime_type = "text/html";
  URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  AddResponse(GURL(url), head, content, status);
}

bool TestURLLoaderFactory::IsPending(const std::string& url,
                                     int* load_flags_out) {
  base::RunLoop().RunUntilIdle();
  for (const auto& candidate : pending_requests_) {
    if (candidate.url == url) {
      if (load_flags_out)
        *load_flags_out = candidate.load_flags;
      return !candidate.client.encountered_error();
    }
  }
  return false;
}

int TestURLLoaderFactory::NumPending() {
  int pending = 0;
  base::RunLoop().RunUntilIdle();
  for (const auto& candidate : pending_requests_) {
    if (!candidate.client.encountered_error())
      ++pending;
  }
  return pending;
}

void TestURLLoaderFactory::ClearResponses() {
  responses_.clear();
}

void TestURLLoaderFactory::SetInterceptor(const Interceptor& interceptor) {
  interceptor_ = interceptor;
}

void TestURLLoaderFactory::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (interceptor_)
    interceptor_.Run(url_request);

  if (CreateLoaderAndStartInternal(url_request.url, client.get()))
    return;

  PendingRequest pending_request;
  pending_request.url = url_request.url;
  pending_request.load_flags = url_request.load_flags;
  pending_request.client = std::move(client);
  pending_request.request_body = std::move(url_request.request_body);
  pending_requests_.push_back(std::move(pending_request));
}

void TestURLLoaderFactory::Clone(mojom::URLLoaderFactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool TestURLLoaderFactory::CreateLoaderAndStartInternal(
    const GURL& url,
    mojom::URLLoaderClient* client) {
  auto it = responses_.find(url);
  if (it == responses_.end())
    return false;

  for (const auto& redirect : it->second.redirects)
    client->OnReceiveRedirect(redirect.first, redirect.second);

  if (it->second.status.error_code == net::OK) {
    client->OnReceiveResponse(it->second.head);
    mojo::DataPipe data_pipe(it->second.content.size());
    uint32_t bytes_written = it->second.content.size();
    CHECK_EQ(MOJO_RESULT_OK, data_pipe.producer_handle->WriteData(
                                 it->second.content.data(), &bytes_written,
                                 MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
    client->OnStartLoadingResponseBody(std::move(data_pipe.consumer_handle));
  }
  client->OnComplete(it->second.status);
  return true;
}

}  // namespace network
