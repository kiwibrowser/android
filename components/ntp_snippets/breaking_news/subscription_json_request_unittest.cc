// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_json_request.h"

#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

namespace internal {

namespace {

using testing::_;
using testing::SaveArg;

// TODO(mamir): Create a test_helper.cc file instead of duplicating all this
// code.
MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected = base::JSONReader::Read(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual = base::JSONReader::ReadAndReturnError(
      arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return *expected == *actual;
}

}  // namespace

class SubscriptionJsonRequestTest : public testing::Test {
 public:
  SubscriptionJsonRequestTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_shared_loader_factory_;
  }

  network::TestURLLoaderFactory* GetURLLoaderFactory() {
    return &test_url_loader_factory_;
  }

  void RespondWithData(const GURL& url, const std::string& data) {
    GetURLLoaderFactory()->AddResponse(url.spec(), data);
    base::RunLoop().RunUntilIdle();
  }

  void RespondWithError(const GURL& url, int error_code) {
    network::URLLoaderCompletionStatus status(error_code);
    test_url_loader_factory_.AddResponse(url, network::ResourceResponseHead(),
                                         "", status);
    base::RunLoop().RunUntilIdle();
  }

  std::string GetBodyFromRequest(const network::ResourceRequest& request) {
    auto body = request.request_body;
    if (!body)
      return std::string();

    CHECK_EQ(1u, body->elements()->size());
    auto& element = body->elements()->at(0);
    CHECK_EQ(network::DataElement::TYPE_BYTES, element.type());
    return std::string(element.bytes(), element.length());
  }

 private:
  base::MessageLoop message_loop_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionJsonRequestTest);
};

TEST_F(SubscriptionJsonRequestTest, BuildRequest) {
  std::string token = "1234567890";
  GURL url("http://valid-url.test");

  base::MockCallback<SubscriptionJsonRequest::CompletedCallback> callback;

  std::string expected_body = R"(
    {
      "token": "1234567890",
      "locale": "en-US",
      "country_code": "us"
    }
  )";
  std::string header;
  std::string actual_body;
  GetURLLoaderFactory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_FALSE(request.headers.GetHeader("Authorization", &header));
        EXPECT_TRUE(request.headers.GetHeader(
            net::HttpRequestHeaders::kContentType, &header));
        EXPECT_EQ(header, "application/json; charset=UTF-8");
        actual_body = GetBodyFromRequest(request);
        EXPECT_THAT(actual_body, EqualsJSON(expected_body));
      }));

  SubscriptionJsonRequest::Builder builder;
  std::unique_ptr<SubscriptionJsonRequest> request =
      builder.SetToken(token)
          .SetUrl(url)
          .SetUrlLoaderFactory(GetSharedURLLoaderFactory())
          .SetLocale("en-US")
          .SetCountryCode("us")
          .Build();
  request->Start(callback.Get());
}

TEST_F(SubscriptionJsonRequestTest, ShouldNotInvokeCallbackWhenCancelled) {
  std::string token = "1234567890";
  GURL url("http://valid-url.test");

  base::MockCallback<SubscriptionJsonRequest::CompletedCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);

  SubscriptionJsonRequest::Builder builder;
  std::unique_ptr<SubscriptionJsonRequest> request =
      builder.SetToken(token)
          .SetUrl(url)
          .SetUrlLoaderFactory(GetSharedURLLoaderFactory())
          .Build();
  GetURLLoaderFactory()->AddResponse(url.spec(), "{}");
  request->Start(callback.Get());

  // Destroy the request before getting any response.
  request.reset();
}

TEST_F(SubscriptionJsonRequestTest, SubscribeWithoutErrors) {
  std::string token = "1234567890";
  GURL url("http://valid-url.test");

  base::MockCallback<SubscriptionJsonRequest::CompletedCallback> callback;
  ntp_snippets::Status status(StatusCode::PERMANENT_ERROR, "initial");
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&status));

  SubscriptionJsonRequest::Builder builder;
  std::unique_ptr<SubscriptionJsonRequest> request =
      builder.SetToken(token)
          .SetUrl(url)
          .SetUrlLoaderFactory(GetSharedURLLoaderFactory())
          .Build();
  request->Start(callback.Get());

  RespondWithData(url, "{}");

  EXPECT_EQ(status.code, StatusCode::SUCCESS);
}

TEST_F(SubscriptionJsonRequestTest, SubscribeWithErrors) {
  std::string token = "1234567890";
  GURL url("http://valid-url.test");

  base::MockCallback<SubscriptionJsonRequest::CompletedCallback> callback;
  ntp_snippets::Status status(StatusCode::SUCCESS, "initial");
  EXPECT_CALL(callback, Run(_)).WillOnce(SaveArg<0>(&status));

  SubscriptionJsonRequest::Builder builder;
  std::unique_ptr<SubscriptionJsonRequest> request =
      builder.SetToken(token)
          .SetUrl(url)
          .SetUrlLoaderFactory(GetSharedURLLoaderFactory())
          .Build();
  request->Start(callback.Get());

  RespondWithError(url, net::ERR_TIMED_OUT);

  EXPECT_EQ(status.code, StatusCode::TEMPORARY_ERROR);
}

}  // namespace internal

}  // namespace ntp_snippets
