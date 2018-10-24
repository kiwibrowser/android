// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class CacheStorageManager;
class MockQuotaManager;
class ServiceWorkerContextWrapper;
struct ServiceWorkerResponse;
class StoragePartition;

// Test DataManager that sets up a CacheStorageManager suited for test
// environments. Tests can also optionally override FillServiceWorkerResponse by
// setting |mock_fill_response| to true.
class BackgroundFetchTestDataManager : public BackgroundFetchDataManager {
 public:
  BackgroundFetchTestDataManager(
      BrowserContext* browser_context,
      StoragePartition* storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      bool mock_fill_response = false);

  ~BackgroundFetchTestDataManager() override;

  bool FillServiceWorkerResponse(const BackgroundFetchRequestInfo& request,
                                 const url::Origin& origin,
                                 ServiceWorkerResponse* response) override;

 private:
  friend class BackgroundFetchDataManagerTest;

  void CreateCacheStorageManager();

  CacheStorageManager* GetCacheStorageManager() override;

  scoped_refptr<MockQuotaManager> mock_quota_manager_;
  std::unique_ptr<CacheStorageManager> cache_manager_;
  BrowserContext* browser_context_;
  StoragePartition* storage_partition_;
  bool mock_fill_response_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchTestDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_TEST_DATA_MANAGER_H_
