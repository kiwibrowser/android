// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/web_package/signed_exchange_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_mock_http_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace content {

namespace {

const uint64_t kSignatureHeaderDate = 1520834000;  // 2018-03-12T05:53:20Z

class NavigationFailureObserver : public WebContentsObserver {
 public:
  explicit NavigationFailureObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~NavigationFailureObserver() override = default;

  void DidStartNavigation(NavigationHandle* handle) override {
    auto throttle = std::make_unique<TestNavigationThrottle>(handle);
    throttle->SetCallback(
        TestNavigationThrottle::WILL_FAIL_REQUEST,
        base::BindRepeating(&NavigationFailureObserver::OnWillFailRequestCalled,
                            base::Unretained(this)));
    handle->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(std::move(throttle)));
  }

  bool did_fail() const { return did_fail_; }

 private:
  void OnWillFailRequestCalled() { did_fail_ = true; }

  bool did_fail_ = false;

  DISALLOW_COPY_AND_ASSIGN(NavigationFailureObserver);
};

}  // namespace

class SignedExchangeRequestHandlerBrowserTest : public ContentBrowserTest {
 public:
  SignedExchangeRequestHandlerBrowserTest()
      : mock_cert_verifier_(std::make_unique<net::MockCertVerifier>()){};

  void SetUp() override {
    SignedExchangeHandler::SetCertVerifierForTesting(mock_cert_verifier_.get());
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Time::UnixEpoch() +
        base::TimeDelta::FromSeconds(kSignatureHeaderDate));
    SetUpFeatures();
    ContentBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    interceptor_.reset();
    SignedExchangeHandler::SetCertVerifierForTesting(nullptr);
    SignedExchangeHandler::SetVerificationTimeForTesting(
        base::Optional<base::Time>());
  }

 protected:
  virtual void SetUpFeatures() {
    feature_list_.InitWithFeatures({features::kSignedHTTPExchange},
                                   {network::features::kNetworkService});
  }

  static scoped_refptr<net::X509Certificate> LoadCertificate(
      const std::string& cert_file) {
    base::ScopedAllowBlockingForTesting allow_io;
    base::FilePath dir_path;
    base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
    dir_path = dir_path.AppendASCII("htxg");

    return net::CreateCertificateChainFromFile(
        dir_path, cert_file, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  }

  void InstallUrlInterceptor(const GURL& url, const std::string& data_path) {
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      if (!interceptor_) {
        interceptor_ =
            std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
                &SignedExchangeRequestHandlerBrowserTest::OnInterceptCallback,
                base::Unretained(this)));
      }
      interceptor_data_path_map_[url] = data_path;
    } else {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::BindOnce(&InstallMockInterceptors, url, data_path));
    }
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;

 private:
  static void InstallMockInterceptors(const GURL& url,
                                      const std::string& data_path) {
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    base::FilePath root_path;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
    net::URLRequestFilter::GetInstance()->AddUrlInterceptor(
        url, net::URLRequestMockHTTPJob::CreateInterceptorForSingleFile(
                 root_path.AppendASCII(data_path)));
  }

  bool OnInterceptCallback(URLLoaderInterceptor::RequestParams* params) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    const auto it = interceptor_data_path_map_.find(params->url_request.url);
    if (it == interceptor_data_path_map_.end())
      return false;
    URLLoaderInterceptor::WriteResponse(it->second, params->client.get());
    return true;
  }

  std::unique_ptr<URLLoaderInterceptor> interceptor_;
  std::map<GURL, std::string> interceptor_data_path_map_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeRequestHandlerBrowserTest);
};

class SignedExchangeRequestHandlerWithNetworkServiceBrowserTest
    : public SignedExchangeRequestHandlerBrowserTest {
  void SetUpFeatures() override {
    feature_list_.InitWithFeatures(
        {features::kSignedHTTPExchange, network::features::kNetworkService},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerBrowserTest, Simple) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/htxg/test.example.org.public.pem.cbor");

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  mock_cert_verifier_->AddResultForCertAndHost(
      original_cert, "test.example.org", dummy_result, net::OK);

  embedded_test_server()->RegisterRequestMonitor(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        if (request.relative_url == "/htxg/test.example.org_test.htxg") {
          const auto& accept_value = request.headers.find("accept")->second;
          EXPECT_THAT(accept_value,
                      ::testing::HasSubstr("application/signed-exchange;v=b1"));
        }
      }));
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/htxg/test.example.org_test.htxg");
  base::string16 title = base::ASCIIToUTF16("https://test.example.org/test/");
  TitleWatcher title_watcher(shell()->web_contents(), title);
  NavigateToURL(shell(), url);
  EXPECT_EQ(title, title_watcher.WaitAndGetTitle());

  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_TRUE(entry->GetSSL().initialized);
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
  ASSERT_TRUE(entry->GetSSL().certificate);

  // "test.example.org.public.pem.cbor" is generated from
  // "prime256v1-sha256.public.pem". So the SHA256 of the certificates must
  // match.
  const net::SHA256HashValue fingerprint =
      net::X509Certificate::CalculateFingerprint256(
          entry->GetSSL().certificate->cert_buffer());
  const net::SHA256HashValue original_fingerprint =
      net::X509Certificate::CalculateFingerprint256(
          original_cert->cert_buffer());
  EXPECT_EQ(original_fingerprint, fingerprint);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerBrowserTest,
                       InvalidContentType) {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/htxg/test.example.org.public.pem.cbor");

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  mock_cert_verifier_->AddResultForCertAndHost(
      original_cert, "test.example.org", dummy_result, net::OK);

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/htxg/test.example.org_test_invalid_content_type.htxg");

  NavigationFailureObserver failure_observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_TRUE(failure_observer.did_fail());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeRequestHandlerBrowserTest, CertNotFound) {
  InstallUrlInterceptor(GURL("https://cert.example.org/cert.msg"),
                        "content/test/data/htxg/404.msg");

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/htxg/test.example.org_test.htxg");

  NavigationFailureObserver failure_observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_TRUE(failure_observer.did_fail());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
}

IN_PROC_BROWSER_TEST_F(
    SignedExchangeRequestHandlerWithNetworkServiceBrowserTest,
    NetworkServiceEnabled) {
  InstallUrlInterceptor(
      GURL("https://test.example.org/cert.msg"),
      "content/test/data/htxg/test.example.org.public.pem.cbor");

  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert =
      LoadCertificate("prime256v1-sha256.public.pem");
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  mock_cert_verifier_->AddResultForCertAndHost(
      original_cert, "test.example.org", dummy_result, net::OK);

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/htxg/test.example.org_test.htxg");

  NavigationFailureObserver failure_observer(shell()->web_contents());
  NavigateToURL(shell(), url);
  EXPECT_TRUE(failure_observer.did_fail());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetVisibleEntry();
  EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
}

}  // namespace content
