// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/mark_request_complete_task.h"

#include "base/barrier_closure.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content {

namespace background_fetch {

MarkRequestCompleteTask::MarkRequestCompleteTask(
    BackgroundFetchDataManager* data_manager,
    BackgroundFetchRegistrationId registration_id,
    scoped_refptr<BackgroundFetchRequestInfo> request_info,
    CacheStorageManager* cache_manager,
    MarkedCompleteCallback callback)
    : DatabaseTask(data_manager),
      registration_id_(registration_id),
      request_info_(std::move(request_info)),
      cache_manager_(cache_manager),
      callback_(std::move(callback)),
      weak_factory_(this) {
  DCHECK(cache_manager_);
}

MarkRequestCompleteTask::~MarkRequestCompleteTask() = default;

void MarkRequestCompleteTask::Start() {
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2u, base::BindOnce(&MarkRequestCompleteTask::CheckAndCallFinished,
                         weak_factory_.GetWeakPtr()));

  StoreResponse(barrier_closure);
  UpdateMetadata(barrier_closure);
}

void MarkRequestCompleteTask::StoreResponse(base::OnceClosure done_closure) {
  auto response = std::make_unique<ServiceWorkerResponse>();

  is_response_successful_ = data_manager()->FillServiceWorkerResponse(
      *request_info_, registration_id_.origin(), response.get());

  // A valid non-empty url is needed if we want to write to the cache.
  if (!request_info_->fetch_request().url.is_valid()) {
    CreateAndStoreCompletedRequest(std::move(done_closure));
    return;
  }

  cache_manager_->OpenCache(
      registration_id_.origin(), CacheStorageOwner::kBackgroundFetch,
      registration_id_.unique_id() /* cache_name */,
      base::BindOnce(&MarkRequestCompleteTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), std::move(response),
                     std::move(done_closure)));
}

void MarkRequestCompleteTask::DidOpenCache(
    std::unique_ptr<ServiceWorkerResponse> response,
    base::OnceClosure done_closure,
    CacheStorageCacheHandle handle,
    blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    // TODO(crbug.com/780025): Log failures to UMA.
    CreateAndStoreCompletedRequest(std::move(done_closure));
    return;
  }
  DCHECK(handle.value());

  auto request = std::make_unique<ServiceWorkerFetchRequest>(
      request_info_->fetch_request());

  // We need to keep the handle refcounted while the write is happening,
  // so it's passed along to the callback.
  handle.value()->Put(
      std::move(request), std::move(response),
      base::BindOnce(&MarkRequestCompleteTask::DidWriteToCache,
                     weak_factory_.GetWeakPtr(), std::move(handle),
                     std::move(done_closure)));
}

void MarkRequestCompleteTask::DidWriteToCache(
    CacheStorageCacheHandle handle,
    base::OnceClosure done_closure,
    blink::mojom::CacheStorageError error) {
  // TODO(crbug.com/780025): Log failures to UMA.
  CreateAndStoreCompletedRequest(std::move(done_closure));
}

void MarkRequestCompleteTask::CreateAndStoreCompletedRequest(
    base::OnceClosure done_closure) {
  completed_request_.set_unique_id(registration_id_.unique_id());
  completed_request_.set_request_index(request_info_->request_index());
  completed_request_.set_serialized_request(
      request_info_->fetch_request().Serialize());
  completed_request_.set_download_guid(request_info_->download_guid());
  completed_request_.set_succeeded(is_response_successful_);

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{CompletedRequestKey(completed_request_.unique_id(),
                            completed_request_.request_index()),
        completed_request_.SerializeAsString()}},
      base::BindOnce(&MarkRequestCompleteTask::DidStoreCompletedRequest,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidStoreCompletedRequest(
    base::OnceClosure done_closure,
    ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
    case DatabaseStatus::kNotFound:
      // TODO(crbug.com/780025): Log failures to UMA.
      std::move(done_closure).Run();
      return;
  }

  // Delete the active request.
  service_worker_context()->ClearRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {ActiveRequestKey(completed_request_.unique_id(),
                        completed_request_.request_index())},
      base::BindOnce(&MarkRequestCompleteTask::DidDeleteActiveRequest,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidDeleteActiveRequest(
    base::OnceClosure done_closure,
    ServiceWorkerStatusCode status) {
  // TODO(crbug.com/780025): Log failures to UMA.
  std::move(done_closure).Run();
}

void MarkRequestCompleteTask::UpdateMetadata(base::OnceClosure done_closure) {
  if (!request_info_->IsResultSuccess() || request_info_->GetFileSize() == 0u) {
    std::move(done_closure).Run();
    return;
  }

  service_worker_context()->GetRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      {RegistrationKey(registration_id_.unique_id())},
      base::BindOnce(&MarkRequestCompleteTask::DidGetMetadata,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidGetMetadata(
    base::OnceClosure done_closure,
    const std::vector<std::string>& data,
    ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kNotFound:
    case DatabaseStatus::kFailed:
      // TODO(crbug.com/780025): Log failures to UMA.
      std::move(done_closure).Run();
      return;
    case DatabaseStatus::kOk:
      DCHECK_EQ(1u, data.size());
      break;
  }

  proto::BackgroundFetchMetadata metadata;
  if (!metadata.ParseFromString(data.front())) {
    NOTREACHED() << "Database is corrupt";  // TODO(crbug.com/780027): Nuke it.
  }

  metadata.mutable_registration()->set_download_total(
      metadata.registration().download_total() + request_info_->GetFileSize());

  service_worker_context()->StoreRegistrationUserData(
      registration_id_.service_worker_registration_id(),
      registration_id_.origin().GetURL(),
      {{RegistrationKey(registration_id_.unique_id()),
        metadata.SerializeAsString()}},
      base::BindOnce(&MarkRequestCompleteTask::DidStoreMetadata,
                     weak_factory_.GetWeakPtr(), std::move(done_closure)));
}

void MarkRequestCompleteTask::DidStoreMetadata(base::OnceClosure done_closure,
                                               ServiceWorkerStatusCode status) {
  // TODO(crbug.com/780025): Log failures to UMA.
  std::move(done_closure).Run();
}

void MarkRequestCompleteTask::CheckAndCallFinished() {
  std::move(callback_).Run();
  Finished();
}

}  // namespace background_fetch

}  // namespace content
