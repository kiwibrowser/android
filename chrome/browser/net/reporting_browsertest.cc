// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class ReportingBrowserTest : public CertVerifierBrowserTest {
 public:
  ReportingBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}
  ~ReportingBrowserTest() override = default;

  void SetUp() override;
  void SetUpOnMainThread() override;

  net::EmbeddedTestServer* server() { return &https_server_; }
  int port() const { return https_server_.port(); }

  net::test_server::ControllableHttpResponse* original_response() {
    return original_response_.get();
  }

  net::test_server::ControllableHttpResponse* upload_response() {
    return upload_response_.get();
  }

  GURL GetReportingEnabledURL() const {
    return GURL(base::StringPrintf("https://example.com:%d/original", port()));
  }

  GURL GetCollectorURL() const {
    return GURL(base::StringPrintf("https://example.com:%d/upload", port()));
  }

  std::string GetReportToHeader() const {
    return "Report-To: {\"endpoints\":[{\"url\":\"" + GetCollectorURL().spec() +
           "\"}],\"max_age\":86400}\r\n";
  }

  std::string GetNELHeader() const {
    return "NEL: "
           "{\"report_to\":\"default\",\"max_age\":86400,\"success_fraction\":"
           "1.0}\r\n";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      original_response_;
  std::unique_ptr<net::test_server::ControllableHttpResponse> upload_response_;

  DISALLOW_COPY_AND_ASSIGN(ReportingBrowserTest);
};

void ReportingBrowserTest::SetUp() {
  scoped_feature_list_.InitWithFeatures(
      {network::features::kReporting, network::features::kNetworkErrorLogging},
      {});
  CertVerifierBrowserTest::SetUp();

  // Make report delivery happen instantly.
  net::ReportingPolicy policy;
  policy.delivery_interval = base::TimeDelta::FromSeconds(0);
  net::ReportingPolicy::UsePolicyForTesting(policy);
}

void ReportingBrowserTest::SetUpOnMainThread() {
  CertVerifierBrowserTest::SetUpOnMainThread();

  host_resolver()->AddRule("*", "127.0.0.1");

  original_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/original");
  upload_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(server(),
                                                                   "/upload");

  // Reporting and NEL will ignore configurations headers if the response
  // doesn't come from an HTTPS origin, or if the origin's certificate is
  // invalud.  Our test certs are valid, so we need a mock certificate verifier
  // to trick the Reporting stack into paying attention to our test headers.
  mock_cert_verifier()->set_default_result(net::OK);
  ASSERT_TRUE(server()->Start());
}

std::unique_ptr<base::Value> ParseReportUpload(const std::string& payload) {
  auto parsed_payload = base::test::ParseJson(payload);
  // Clear out any non-reproducible fields.
  for (auto& report : parsed_payload->GetList()) {
    report.RemoveKey("age");
    report.RemovePath({"report", "elapsed_time"});
  }
  return parsed_payload;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ReportingBrowserTest, TestReportingHeadersProcessed) {
  NavigateParams params(browser(), GetReportingEnabledURL(),
                        ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  original_response()->WaitForRequest();
  original_response()->Send("HTTP/1.1 204 OK\r\n");
  original_response()->Send(GetReportToHeader());
  original_response()->Send(GetNELHeader());
  original_response()->Send("\r\n");
  original_response()->Done();

  upload_response()->WaitForRequest();
  auto actual = ParseReportUpload(upload_response()->http_request()->content);
  upload_response()->Send("HTTP/1.1 204 OK\r\n");
  upload_response()->Send("\r\n");
  upload_response()->Done();

  // Verify the contents of the report that we received.
  EXPECT_TRUE(actual != nullptr);
  auto expected = base::test::ParseJson(base::StringPrintf(
      R"json(
        [
          {
            "report": {
              "protocol": "http/1.1",
              "referrer": "",
              "sampling_fraction": 1.0,
              "server_ip": "127.0.0.1",
              "status_code": 204,
              "type": "ok",
              "uri": "https://example.com:%d/original",
            },
            "type": "network-error",
            "url": "https://example.com:%d/original",
          },
        ]
      )json",
      port(), port()));
  EXPECT_EQ(*expected, *actual);
}
