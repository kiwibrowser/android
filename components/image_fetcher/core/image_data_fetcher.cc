// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_data_fetcher.h"

#include <utility>

#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"  // for ReferrerPolicy
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

using data_use_measurement::DataUseUserData;

namespace {

const char kContentLocationHeader[] = "Content-Location";

}  // namespace

namespace image_fetcher {

// An active image URL fetcher request. The struct contains the related requests
// state.
struct ImageDataFetcher::ImageDataFetcherRequest {
  ImageDataFetcherRequest(ImageDataFetcherCallback callback,
                          std::unique_ptr<network::SimpleURLLoader> loader)
      : callback(std::move(callback)), loader(std::move(loader)) {}

  ~ImageDataFetcherRequest() {}

  // The callback to run after the image data was fetched. The callback will
  // be run even if the image data could not be fetched successfully.
  ImageDataFetcherCallback callback;

  std::unique_ptr<network::SimpleURLLoader> loader;
};

ImageDataFetcher::ImageDataFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      data_use_service_name_(DataUseUserData::IMAGE_FETCHER_UNTAGGED) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ImageDataFetcher::~ImageDataFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ImageDataFetcher::SetDataUseServiceName(
    DataUseServiceName data_use_service_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_use_service_name_ = data_use_service_name;
}

void ImageDataFetcher::SetImageDownloadLimit(
    base::Optional<int64_t> max_download_bytes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_download_bytes_ = max_download_bytes;
}

void ImageDataFetcher::FetchImageData(
    const GURL& image_url,
    ImageDataFetcherCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  FetchImageData(
      image_url, std::move(callback), /*referrer=*/std::string(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      traffic_annotation);
}

void ImageDataFetcher::FetchImageData(
    const GURL& image_url,
    ImageDataFetcherCallback callback,
    const std::string& referrer,
    net::URLRequest::ReferrerPolicy referrer_policy,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = image_url;
  request->referrer_policy = referrer_policy;
  request->referrer = GURL(referrer);
  request->load_flags = net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES |
                        net::LOAD_DO_NOT_SEND_AUTH_DATA;

  // TODO(https://crbug.com/808498) re-add data use measurement once
  // SimpleURLLoader supports it.  Parameter:
  // data_use_service_name_

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  // For compatibility in error handling. This is a little wasteful since the
  // body will get thrown out anyway, though.
  loader->SetAllowHttpErrorResults(true);

  if (max_download_bytes_.has_value()) {
    loader->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&ImageDataFetcher::OnURLLoaderComplete,
                       base::Unretained(this), loader.get()),
        max_download_bytes_.value());
  } else {
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&ImageDataFetcher::OnURLLoaderComplete,
                       base::Unretained(this), loader.get()));
  }

  std::unique_ptr<ImageDataFetcherRequest> request_track(
      new ImageDataFetcherRequest(std::move(callback), std::move(loader)));

  pending_requests_[request_track->loader.get()] = std::move(request_track);
}

void ImageDataFetcher::OnURLLoaderComplete(
    const network::SimpleURLLoader* source,
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_requests_.find(source) != pending_requests_.end());
  bool success = source->NetError() == net::OK;

  RequestMetadata metadata;
  if (success && source->ResponseInfo() && source->ResponseInfo()->headers) {
    net::HttpResponseHeaders* headers = source->ResponseInfo()->headers.get();
    metadata.mime_type = source->ResponseInfo()->mime_type;
    metadata.http_response_code = headers->response_code();
    // Just read the first value-pair for this header (not caring about |iter|).
    headers->EnumerateHeader(
        /*iter=*/nullptr, kContentLocationHeader,
        &metadata.content_location_header);
    success &= (metadata.http_response_code == net::HTTP_OK);
  }

  std::string image_data;
  if (success && response_body) {
    image_data = std::move(*response_body);
  }
  FinishRequest(source, metadata, image_data);
}

void ImageDataFetcher::FinishRequest(const network::SimpleURLLoader* source,
                                     const RequestMetadata& metadata,
                                     const std::string& image_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto request_iter = pending_requests_.find(source);
  DCHECK(request_iter != pending_requests_.end());
  std::move(request_iter->second->callback).Run(image_data, metadata);
  pending_requests_.erase(request_iter);
}

void ImageDataFetcher::InjectResultForTesting(const RequestMetadata& metadata,
                                              const std::string& image_data) {
  DCHECK_EQ(pending_requests_.size(), 1u);
  FinishRequest(pending_requests_.begin()->first, metadata, image_data);
}

}  // namespace image_fetcher
