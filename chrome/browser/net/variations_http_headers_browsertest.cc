// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <map>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class VariationsHttpHeadersBrowserTest : public InProcessBrowserTest {
 public:
  VariationsHttpHeadersBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}
  ~VariationsHttpHeadersBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    server()->RegisterRequestHandler(
        base::BindRepeating(&VariationsHttpHeadersBrowserTest::RequestHandler,
                            base::Unretained(this)));

    ASSERT_TRUE(server()->Start());

    // Set up some fake variations.
    auto* variations_provider =
        variations::VariationsHttpHeaderProvider::GetInstance();
    variations_provider->ForceVariationIds({"12", "456", "t789"}, "");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer* server() { return &https_server_; }

  GURL GetGoogleRedirectUrl1() const {
    return GURL(base::StringPrintf("https://www.google.com:%d/redirect",
                                   https_server_.port()));
  }

  GURL GetGoogleRedirectUrl2() const {
    return GURL(base::StringPrintf("https://www.google.com:%d/redirect2",
                                   https_server_.port()));
  }

  GURL GetExampleUrl() const {
    return GURL(base::StringPrintf("https://www.example.com:%d/landing.html",
                                   https_server_.port()));
  }

  // Returns whether a given |header| has been received for a |url|. Note that
  // false is returned if the |url| has not been observed.
  bool HasReceivedHeader(const GURL& url, const std::string& header) const {
    auto it = received_headers_.find(url);
    if (it == received_headers_.end())
      return false;
    return it->second.find(header) != it->second.end();
  }

  bool FetchResource(const GURL& url) {
    if (!url.is_valid())
      return false;
    std::string script(
        "var xhr = new XMLHttpRequest();"
        "xhr.open('GET', '");
    script += url.spec() +
              "', true);"
              "xhr.onload = function (e) {"
              "  if (xhr.readyState === 4) {"
              "    window.domAutomationController.send(xhr.status === 200);"
              "  }"
              "};"
              "xhr.onerror = function () {"
              "  window.domAutomationController.send(false);"
              "};"
              "xhr.send(null)";
    return ExecuteScript(script);
  }

 private:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool ExecuteScript(const std::string& script) {
    bool xhr_result = false;
    // The JS call will fail if disallowed because the process will be killed.
    bool execute_result =
        ExecuteScriptAndExtractBool(GetWebContents(), script, &xhr_result);
    return xhr_result && execute_result;
  }

  // Custom request handler that record request headers and simulates a redirect
  // from google.com to example.com.
  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request);

  net::EmbeddedTestServer https_server_;

  // Stores the observed HTTP Request headers.
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_;

  DISALLOW_COPY_AND_ASSIGN(VariationsHttpHeadersBrowserTest);
};

class BlockingURLFetcherDelegate : public net::URLFetcherDelegate {
 public:
  BlockingURLFetcherDelegate() = default;
  ~BlockingURLFetcherDelegate() override = default;

  void OnURLFetchComplete(const net::URLFetcher* source) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

  void AwaitResponse() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BlockingURLFetcherDelegate);
};

std::unique_ptr<net::test_server::HttpResponse>
VariationsHttpHeadersBrowserTest::RequestHandler(
    const net::test_server::HttpRequest& request) {
  // Retrieve the host name (without port) from the request headers.
  std::string host = "";
  if (request.headers.find("Host") != request.headers.end())
    host = request.headers.find("Host")->second;
  if (host.find(':') != std::string::npos)
    host = host.substr(0, host.find(':'));

  // Recover the original URL of the request by replacing the host name in
  // request.GetURL() (which is 127.0.0.1) with the host name from the request
  // headers.
  url::Replacements<char> replacements;
  replacements.SetHost(host.c_str(), url::Component(0, host.length()));
  GURL original_url = request.GetURL().ReplaceComponents(replacements);

  // Memorize the request headers for this URL for later verification.
  received_headers_[original_url] = request.headers;

  // Set up a test server that redirects according to the
  // following redirect chain:
  // https://www.google.com:<port>/redirect
  // --> https://www.google.com:<port>/redirect2
  // --> https://www.example.com:<port>/
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  if (request.relative_url == GetGoogleRedirectUrl1().path()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetGoogleRedirectUrl2().spec());
  } else if (request.relative_url == GetGoogleRedirectUrl2().path()) {
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", GetExampleUrl().spec());
  } else if (request.relative_url == GetExampleUrl().path()) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
  } else {
    http_response->set_code(net::HTTP_NO_CONTENT);
  }
  return http_response;
}

}  // namespace

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to network requests to Google but stripped on redirects.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromResourceRequest) {
  ui_test_utils::NavigateToURL(browser(), GetGoogleRedirectUrl1());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

// Verify in an integration that that the variations header (X-Client-Data) is
// correctly attached and stripped from network requests that are triggered via
// a URLFetcher.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromInternalRequest) {
  BlockingURLFetcherDelegate delegate;

  GURL url = GetGoogleRedirectUrl1();
  std::unique_ptr<net::URLFetcher> fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, &delegate);
  net::HttpRequestHeaders headers;
  variations::AppendVariationHeaders(url, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo, &headers);
  fetcher->SetRequestContext(browser()->profile()->GetRequestContext());
  fetcher->SetExtraRequestHeaders(headers.ToString());
  fetcher->Start();

  delegate.AwaitResponse();

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

// Verify in an integration that that the variations header (X-Client-Data) is
// correctly attached and stripped from network requests that are triggered via
// the network service.
IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromNetworkService) {
  content::StoragePartition* partition =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile());
  network::mojom::NetworkContext* network_context =
      partition->GetNetworkContext();
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context,
                                               GetGoogleRedirectUrl1()));

  // TODO(crbug.com/794644) Once the network service stack starts injecting
  // X-Client-Data headers, the following expectations should be used.
  EXPECT_FALSE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  /*
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
  */
}

IN_PROC_BROWSER_TEST_F(VariationsHttpHeadersBrowserTest,
                       TestStrippingHeadersFromSubresourceRequest) {
  GURL url = server()->GetURL("/simple_page.html");
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_TRUE(FetchResource(GetGoogleRedirectUrl1()));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithProfileNetworkContext) {
  GURL url = GetGoogleRedirectUrl1();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  variations::AppendVariationHeaders(url, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo,
                                     &resource_request->headers);

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  content::StoragePartition* partition =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile());
  network::SharedURLLoaderFactory* loader_factory =
      partition->GetURLLoaderFactoryForBrowserProcess().get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallback());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

IN_PROC_BROWSER_TEST_F(
    VariationsHttpHeadersBrowserTest,
    TestStrippingHeadersFromRequestUsingSimpleURLLoaderWithGlobalSystemNetworkContext) {
  GURL url = GetGoogleRedirectUrl1();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  variations::AppendVariationHeaders(url, variations::InIncognito::kNo,
                                     variations::SignedIn::kNo,
                                     &resource_request->headers);

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  network::SharedURLLoaderFactory* loader_factory =
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory()
          .get();
  content::SimpleURLLoaderTestHelper loader_helper;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      loader_factory, loader_helper.GetCallback());

  // Wait for the response to complete.
  loader_helper.WaitForCallback();
  EXPECT_TRUE(loader_helper.response_body());

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}
