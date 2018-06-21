// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_MARK_REQUEST_COMPLETE_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_MARK_REQUEST_COMPLETE_TASK_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch.pb.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/storage/database_task.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/common/service_worker/service_worker_status_code.h"

namespace content {

class CacheStorageManager;

namespace background_fetch {

// Moves the request from an active state to a complete state. Stores the
// download response in cache storage.
class MarkRequestCompleteTask : public DatabaseTask {
 public:
  using MarkedCompleteCallback = base::OnceCallback<void()>;

  MarkRequestCompleteTask(
      BackgroundFetchDataManager* data_manager,
      BackgroundFetchRegistrationId registration_id,
      scoped_refptr<BackgroundFetchRequestInfo> request_info,
      CacheStorageManager* cache_manager,
      MarkedCompleteCallback callback);

  ~MarkRequestCompleteTask() override;

  // DatabaseTask implementation:
  void Start() override;

 private:
  void StoreResponse(base::OnceClosure done_closure);

  void DidOpenCache(std::unique_ptr<ServiceWorkerResponse> response,
                    base::OnceClosure done_closure,
                    CacheStorageCacheHandle handle,
                    blink::mojom::CacheStorageError error);

  void DidWriteToCache(CacheStorageCacheHandle handle,
                       base::OnceClosure done_closure,
                       blink::mojom::CacheStorageError error);

  void CreateAndStoreCompletedRequest(base::OnceClosure done_closure);

  void DidStoreCompletedRequest(base::OnceClosure done_closure,
                                ServiceWorkerStatusCode status);

  void DidDeleteActiveRequest(base::OnceClosure done_closure,
                              ServiceWorkerStatusCode status);

  void UpdateMetadata(base::OnceClosure done_closure);

  void DidGetMetadata(base::OnceClosure done_closure,
                      const std::vector<std::string>& data,
                      ServiceWorkerStatusCode status);

  void DidStoreMetadata(base::OnceClosure done_closure,
                        ServiceWorkerStatusCode status);

  void CheckAndCallFinished();

  BackgroundFetchRegistrationId registration_id_;
  scoped_refptr<BackgroundFetchRequestInfo> request_info_;
  CacheStorageManager* cache_manager_;
  MarkedCompleteCallback callback_;

  proto::BackgroundFetchCompletedRequest completed_request_;
  bool is_response_successful_;

  base::WeakPtrFactory<MarkRequestCompleteTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(MarkRequestCompleteTask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_MARK_REQUEST_COMPLETE_TASK_H_