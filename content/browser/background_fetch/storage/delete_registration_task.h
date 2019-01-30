// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DELETE_REGISTRATION_TASK_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DELETE_REGISTRATION_TASK_H_

#include <string>
#include <vector>

#include "content/browser/background_fetch/storage/database_task.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/platform/modules/cache_storage/cache_storage.mojom.h"
#include "url/origin.h"

namespace content {

class CacheStorageManager;

namespace background_fetch {

// Deletes Background Fetch registration entries from the database.
class DeleteRegistrationTask : public background_fetch::DatabaseTask {
 public:
  DeleteRegistrationTask(BackgroundFetchDataManager* data_manager,
                         int64_t service_worker_registration_id,
                         const url::Origin& origin,
                         const std::string& unique_id,
                         CacheStorageManager* cache_manager,
                         HandleBackgroundFetchErrorCallback callback);

  ~DeleteRegistrationTask() override;

  void Start() override;

 private:
  void DidGetRegistration(base::OnceClosure done_closure,
                          const std::vector<std::string>& data,
                          ServiceWorkerStatusCode status);

  void DidDeleteRegistration(base::OnceClosure done_closure,
                             ServiceWorkerStatusCode status);

  void DidDeleteCache(base::OnceClosure done_closure,
                      blink::mojom::CacheStorageError error);

  void FinishTask();

  int64_t service_worker_registration_id_;
  url::Origin origin_;
  std::string unique_id_;
  CacheStorageManager* cache_manager_;
  HandleBackgroundFetchErrorCallback callback_;

  // The error to report once all async work is completed.
  blink::mojom::BackgroundFetchError error_ =
      blink::mojom::BackgroundFetchError::NONE;

  base::WeakPtrFactory<DeleteRegistrationTask> weak_factory_;  // Keep as last.

  DISALLOW_COPY_AND_ASSIGN(DeleteRegistrationTask);
};

}  // namespace background_fetch

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_DELETE_REGISTRATION_TASK_H_
