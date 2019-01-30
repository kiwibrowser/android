// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blob_reader.h"

#include <limits>
#include <utility>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

BlobReader::BlobReader(content::BrowserContext* browser_context,
                       const std::string& blob_uuid,
                       BlobReadCallback callback)
    : BlobReader(
          content::BrowserContext::GetBlobPtr(browser_context, blob_uuid),
          std::move(callback)) {}

BlobReader::BlobReader(blink::mojom::BlobPtr blob, BlobReadCallback callback)
    : callback_(std::move(callback)), blob_(std::move(blob)), binding_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  blob_.set_connection_error_handler(
      base::BindOnce(&BlobReader::Failed, base::Unretained(this)));
}

BlobReader::~BlobReader() { DCHECK_CURRENTLY_ON(content::BrowserThread::UI); }

void BlobReader::SetByteRange(int64_t offset, int64_t length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK_GE(offset, 0);
  CHECK_GT(length, 0);
  CHECK_LE(offset, std::numeric_limits<int64_t>::max() - length);

  read_range_ = Range{offset, length};
}

void BlobReader::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  MojoResult result =
      CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
  if (result != MOJO_RESULT_OK) {
    Failed();
    return;
  }
  blink::mojom::BlobReaderClientPtr client_ptr;
  binding_.Bind(MakeRequest(&client_ptr));
  if (read_range_) {
    blob_->ReadRange(read_range_->offset, read_range_->length,
                     std::move(producer_handle), std::move(client_ptr));
  } else {
    blob_->ReadAll(std::move(producer_handle), std::move(client_ptr));
  }
  data_pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(consumer_handle));
}

void BlobReader::OnCalculatedSize(uint64_t total_size,
                                  uint64_t expected_content_size) {
  blob_length_ = total_size;
  if (data_complete_)
    Succeeded();
}

void BlobReader::OnDataAvailable(const void* data, size_t num_bytes) {
  if (!blob_data_)
    blob_data_ = std::make_unique<std::string>();
  blob_data_->append(static_cast<const char*>(data), num_bytes);
}

void BlobReader::OnDataComplete() {
  data_complete_ = true;
  if (blob_length_)
    Succeeded();
}

void BlobReader::Failed() {
  std::move(callback_).Run(std::make_unique<std::string>(), 0);
  delete this;
}

void BlobReader::Succeeded() {
  std::move(callback_).Run(std::move(blob_data_), *blob_length_);
  delete this;
}
