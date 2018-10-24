// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_data_fetcher.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kImageURL[] = "http://www.example.com/image";
const char kURLResponseData[] = "EncodedImageData";

}  // namespace

namespace image_fetcher {

class ImageDataFetcherTest : public testing::Test {
 public:
  ImageDataFetcherTest()
      : shared_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        image_data_fetcher_(shared_factory_) {}
  ~ImageDataFetcherTest() override {}

  MOCK_METHOD2(OnImageDataFetched,
               void(const std::string&, const RequestMetadata&));

  MOCK_METHOD2(OnImageDataFetchedFailedRequest,
               void(const std::string&, const RequestMetadata&));

  MOCK_METHOD2(OnImageDataFetchedMultipleRequests,
               void(const std::string&, const RequestMetadata&));

 protected:
  base::MessageLoop message_loop_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  ImageDataFetcher image_data_fetcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDataFetcherTest);
};

TEST_F(ImageDataFetcherTest, FetchImageData) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_OK;
  EXPECT_CALL(*this, OnImageDataFetched(content, expected_metadata));

  // Check to make sure the request is pending with proper flags, and
  // provide a response.
  int pending_load_flags = 0;
  EXPECT_TRUE(
      test_url_loader_factory_.IsPending(kImageURL, &pending_load_flags));
  EXPECT_TRUE(pending_load_flags & net::LOAD_DO_NOT_SEND_COOKIES);
  EXPECT_TRUE(pending_load_flags & net::LOAD_DO_NOT_SAVE_COOKIES);
  EXPECT_TRUE(pending_load_flags & net::LOAD_DO_NOT_SEND_AUTH_DATA);

  network::ResourceResponseHead head;
  std::string raw_header =
      "HTTP/1.1 200 OK\n"
      "Content-type: image/png\n\n";
  head.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(raw_header.c_str(), raw_header.size()));
  head.mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), head, content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_NotFound) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_NOT_FOUND;
  // For 404, expect an empty result even though correct image data is sent.
  EXPECT_CALL(*this, OnImageDataFetched(std::string(), expected_metadata));

  // Check to make sure the request is pending, and provide a response.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));

  network::ResourceResponseHead head;
  std::string raw_header =
      "HTTP/1.1 404 Not Found\n"
      "Content-type: image/png\n\n";
  head.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(raw_header.c_str(), raw_header.size()));
  head.mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), head, content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_WithContentLocation) {
  std::string content = kURLResponseData;

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  RequestMetadata expected_metadata;
  expected_metadata.mime_type = std::string("image/png");
  expected_metadata.http_response_code = net::HTTP_NOT_FOUND;
  expected_metadata.content_location_header = "http://test-location/image.png";
  // For 404, expect an empty result even though correct image data is sent.
  EXPECT_CALL(*this, OnImageDataFetched(std::string(), expected_metadata));

  // Check to make sure the request is pending, and provide a response.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));

  network::ResourceResponseHead head;
  std::string raw_header =
      "HTTP/1.1 404 Not Found\n"
      "Content-type: image/png\n"
      "Content-location: http://test-location/image.png\n\n";
  head.headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(raw_header.c_str(), raw_header.size()));
  head.mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.decoded_body_length = content.size();
  test_url_loader_factory_.AddResponse(GURL(kImageURL), head, content, status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_FailedRequest) {
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetchedFailedRequest,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  RequestMetadata expected_metadata;
  expected_metadata.http_response_code = net::URLFetcher::RESPONSE_CODE_INVALID;
  EXPECT_CALL(
      *this, OnImageDataFetchedFailedRequest(std::string(), expected_metadata));

  // Check to make sure the request is pending, and provide a response.
  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));

  network::ResourceResponseHead head;
  head.mime_type = "image/png";
  network::URLLoaderCompletionStatus status;
  status.error_code = net::ERR_INVALID_URL;
  test_url_loader_factory_.AddResponse(GURL(kImageURL), head, "", status);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_MultipleRequests) {
  EXPECT_CALL(*this, OnImageDataFetchedMultipleRequests(testing::_, testing::_))
      .Times(2);

  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetchedMultipleRequests,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetchedMultipleRequests,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  // Multiple calls to FetchImageData for the same URL will result in
  // multiple URLFetchers being created.
  EXPECT_EQ(2, test_url_loader_factory_.NumPending());
  test_url_loader_factory_.AddResponse(kImageURL, "");
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDataFetcherTest, FetchImageData_CancelFetchIfImageExceedsMaxSize) {
  const int64_t kMaxDownloadBytes = 1024;
  std::string oversize_download(kMaxDownloadBytes + 1, '#');

  image_data_fetcher_.SetImageDownloadLimit(kMaxDownloadBytes);
  image_data_fetcher_.FetchImageData(
      GURL(kImageURL),
      base::BindOnce(&ImageDataFetcherTest::OnImageDataFetched,
                     base::Unretained(this)),
      TRAFFIC_ANNOTATION_FOR_TESTS);

  // Fetching an oversized image will behave like any other failed request.
  // There will be exactly one call to OnImageDataFetched containing a response
  // code that would be impossible for a completed fetch.
  RequestMetadata expected_metadata;
  expected_metadata.http_response_code = net::URLFetcher::RESPONSE_CODE_INVALID;
  EXPECT_CALL(*this, OnImageDataFetched(std::string(), expected_metadata));

  EXPECT_TRUE(test_url_loader_factory_.IsPending(kImageURL));
  test_url_loader_factory_.AddResponse(kImageURL, oversize_download);
  base::RunLoop().RunUntilIdle();
}

}  // namespace image_fetcher
