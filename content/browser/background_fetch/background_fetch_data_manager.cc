// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "content/browser/background_fetch/background_fetch_constants.h"
#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/storage/cleanup_task.h"
#include "content/browser/background_fetch/storage/create_metadata_task.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/background_fetch/storage/delete_registration_task.h"
#include "content/browser/background_fetch/storage/get_developer_ids_task.h"
#include "content/browser/background_fetch/storage/get_metadata_task.h"
#include "content/browser/background_fetch/storage/get_num_requests_task.h"
#include "content/browser/background_fetch/storage/get_settled_fetches_task.h"
#include "content/browser/background_fetch/storage/mark_registration_for_deletion_task.h"
#include "content/browser/background_fetch/storage/mark_request_complete_task.h"
#include "content/browser/background_fetch/storage/start_next_pending_request_task.h"
#include "content/browser/background_fetch/storage/update_registration_ui_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content {

namespace {

// Returns whether the response contained in the Background Fetch |request| is
// considered OK. See https://fetch.spec.whatwg.org/#ok-status aka a successful
// 2xx status per https://tools.ietf.org/html/rfc7231#section-6.3.
bool IsOK(const BackgroundFetchRequestInfo& request) {
  int status = request.GetResponseCode();
  return status >= 200 && status < 300;
}

// Helper function to convert a BackgroundFetchRegistration proto into a
// BackgroundFetchRegistration struct, and call the appropriate callback.
void GetRegistrationFromMetadata(
    BackgroundFetchDataManager::GetRegistrationCallback callback,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<proto::BackgroundFetchMetadata> metadata_proto) {
  if (!metadata_proto) {
    std::move(callback).Run(error, nullptr);
    return;
  }

  const auto& registration_proto = metadata_proto->registration();
  auto registration = std::make_unique<BackgroundFetchRegistration>();
  registration->developer_id = registration_proto.developer_id();
  registration->unique_id = registration_proto.unique_id();
  // TODO(crbug.com/774054): Uploads are not yet supported.
  registration->upload_total = registration_proto.upload_total();
  registration->uploaded = registration_proto.uploaded();
  registration->download_total = registration_proto.download_total();
  registration->downloaded = registration_proto.downloaded();

  std::move(callback).Run(error, std::move(registration));
}

}  // namespace

BackgroundFetchDataManager::BackgroundFetchDataManager(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<CacheStorageContextImpl> cache_storage_context)
    : service_worker_context_(std::move(service_worker_context)),
      cache_storage_context_(std::move(cache_storage_context)),
      weak_ptr_factory_(this) {
  // Constructed on the UI thread, then used on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  // Store the blob storage context for the given |browser_context|.
  blob_storage_context_ =
      base::WrapRefCounted(ChromeBlobStorageContext::GetFor(browser_context));
  DCHECK(blob_storage_context_);

  BrowserThread::PostAfterStartupTask(
      FROM_HERE, BrowserThread::GetTaskRunnerForThread(BrowserThread::IO),
      // Normally weak pointers must be obtained on the IO thread, but it's ok
      // here as the factory cannot be destroyed before the constructor ends.
      base::BindOnce(&BackgroundFetchDataManager::Cleanup,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchDataManager::Cleanup() {
  AddDatabaseTask(std::make_unique<background_fetch::CleanupTask>(
      this, GetCacheStorageManager()));
}

BackgroundFetchDataManager::~BackgroundFetchDataManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchDataManager::CreateRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const SkBitmap& icon,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto registration_callback =
      base::BindOnce(&GetRegistrationFromMetadata, std::move(callback));
  AddDatabaseTask(std::make_unique<background_fetch::CreateMetadataTask>(
      this, registration_id, requests, options,
      std::move(registration_callback)));
}

void BackgroundFetchDataManager::GetMetadata(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetMetadataTask>(
      this, service_worker_registration_id, origin, developer_id,
      std::move(callback)));
}

void BackgroundFetchDataManager::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto registration_callback =
      base::BindOnce(&GetRegistrationFromMetadata, std::move(callback));
  GetMetadata(service_worker_registration_id, origin, developer_id,
              std::move(registration_callback));
}

void BackgroundFetchDataManager::UpdateRegistrationUI(
    const BackgroundFetchRegistrationId& registration_id,
    const std::string& title,
    blink::mojom::BackgroundFetchService::UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::UpdateRegistrationUITask>(
      this, registration_id, title, std::move(callback)));
}

void BackgroundFetchDataManager::PopNextRequest(
    const BackgroundFetchRegistrationId& registration_id,
    NextRequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto start_next_request = base::BindOnce(
      &BackgroundFetchDataManager::AddStartNextPendingRequestTask,
      weak_ptr_factory_.GetWeakPtr(),
      registration_id.service_worker_registration_id(), std::move(callback));

  // Get the associated metadata, and add a StartNextPendingRequestTask.
  GetMetadata(registration_id.service_worker_registration_id(),
              registration_id.origin(), registration_id.developer_id(),
              std::move(start_next_request));
}

void BackgroundFetchDataManager::AddStartNextPendingRequestTask(
    int64_t service_worker_registration_id,
    NextRequestCallback callback,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<proto::BackgroundFetchMetadata> metadata) {
  if (!metadata) {
    // Stop giving out requests as registration aborted (or otherwise finished).
    std::move(callback).Run(nullptr /* request */);
    return;
  }
  DCHECK_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  AddDatabaseTask(
      std::make_unique<background_fetch::StartNextPendingRequestTask>(
          this, service_worker_registration_id, std::move(metadata),
          std::move(callback)));
}

void BackgroundFetchDataManager::MarkRequestAsComplete(
    const BackgroundFetchRegistrationId& registration_id,
    BackgroundFetchRequestInfo* request,
    BackgroundFetchScheduler::MarkedCompleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::MarkRequestCompleteTask>(
      this, registration_id, request, GetCacheStorageManager(),
      std::move(callback)));
}

void BackgroundFetchDataManager::GetSettledFetchesForRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    SettledFetchesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetSettledFetchesTask>(
      this, registration_id, GetCacheStorageManager(), std::move(callback)));
}

bool BackgroundFetchDataManager::FillServiceWorkerResponse(
    const BackgroundFetchRequestInfo& request,
    const url::Origin& origin,
    ServiceWorkerResponse* response) {
  DCHECK(response);

  response->url_list = request.GetURLChain();
  response->response_type = network::mojom::FetchResponseType::kDefault;
  // TODO(crbug.com/838837): settled_fetch.response.error
  response->response_time = request.GetResponseTime();
  // TODO(crbug.com/838837): settled_fetch.response.cors_exposed_header_names

  BackgroundFetchCrossOriginFilter filter(origin, request);
  if (!filter.CanPopulateBody()) {
    // TODO(crbug.com/711354): Consider Background Fetches as failed when the
    // response cannot be relayed to the developer.
    return false;
  }

  // Include the status code, status text and the response's body as a blob
  // when this is allowed by the CORS protocol.
  response->status_code = request.GetResponseCode();
  response->status_text = request.GetResponseText();
  response->headers.insert(request.GetResponseHeaders().begin(),
                           request.GetResponseHeaders().end());

  if (request.GetFileSize() > 0) {
    DCHECK(!request.GetFilePath().empty());
    DCHECK(blob_storage_context_);

    auto blob_builder =
        std::make_unique<storage::BlobDataBuilder>(base::GenerateGUID());
    blob_builder->AppendFile(request.GetFilePath(), 0 /* offset */,
                             request.GetFileSize(),
                             base::Time() /* expected_modification_time */);

    auto blob_data_handle = GetBlobStorageContext(blob_storage_context_.get())
                                ->AddFinishedBlob(std::move(blob_builder));

    // TODO(peter): Appropriately handle !blob_data_handle
    if (blob_data_handle) {
      response->blob_uuid = blob_data_handle->uuid();
      response->blob_size = blob_data_handle->size();
      blink::mojom::BlobPtr blob_ptr;
      storage::BlobImpl::Create(
          std::make_unique<storage::BlobDataHandle>(*blob_data_handle),
          MakeRequest(&blob_ptr));

      response->blob =
          base::MakeRefCounted<storage::BlobHandle>(std::move(blob_ptr));
    }
  }

  return IsOK(request);
}

void BackgroundFetchDataManager::MarkRegistrationForDeletion(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(
      std::make_unique<background_fetch::MarkRegistrationForDeletionTask>(
          this, registration_id, std::move(callback)));
}

void BackgroundFetchDataManager::DeleteRegistration(
    const BackgroundFetchRegistrationId& registration_id,
    HandleBackgroundFetchErrorCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::DeleteRegistrationTask>(
      this, registration_id.service_worker_registration_id(),
      registration_id.origin(), registration_id.unique_id(),
      GetCacheStorageManager(), std::move(callback)));
}

void BackgroundFetchDataManager::GetDeveloperIdsForServiceWorker(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    blink::mojom::BackgroundFetchService::GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetDeveloperIdsTask>(
      this, service_worker_registration_id, origin, std::move(callback)));
}

void BackgroundFetchDataManager::GetNumCompletedRequests(
    const BackgroundFetchRegistrationId& registration_id,
    NumRequestsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  AddDatabaseTask(std::make_unique<background_fetch::GetNumRequestsTask>(
      this, registration_id, background_fetch::RequestType::kCompleted,
      std::move(callback)));
}

CacheStorageManager* BackgroundFetchDataManager::GetCacheStorageManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorageManager* manager = cache_storage_context_->cache_manager();
  DCHECK(manager);

  return manager;
}

void BackgroundFetchDataManager::AddDatabaseTask(
    std::unique_ptr<background_fetch::DatabaseTask> task) {
  database_tasks_.push(std::move(task));
  if (database_tasks_.size() == 1)
    database_tasks_.front()->Start();
}

void BackgroundFetchDataManager::OnDatabaseTaskFinished(
    background_fetch::DatabaseTask* task) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!database_tasks_.empty());
  DCHECK_EQ(database_tasks_.front().get(), task);

  database_tasks_.pop();
  if (!database_tasks_.empty())
    database_tasks_.front()->Start();
}

}  // namespace content
