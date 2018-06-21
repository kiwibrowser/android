// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_H_

#include <string>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/download/database/download_namespace.h"

namespace download {

struct DownloadDBEntry;

// A backing storage for persisting DownloadDBEntry objects.
class DownloadDB {
 public:
  using LoadEntriesCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<std::vector<DownloadDBEntry>> entries)>;
  using InitializeCallback = base::OnceCallback<void(bool success)>;

  virtual ~DownloadDB() = default;

  // Returns whether or not this object is initialized and can be interracted
  // with.
  virtual bool IsInitialized() = 0;

  // Initializes this db asynchronously, callback will be run on completion.
  virtual void Initialize(InitializeCallback callback) = 0;

  // Adds or updates |entry| in the storage asynchronously and returns whether
  // or not that was successful.
  virtual void AddOrReplace(const DownloadDBEntry& entry) = 0;

  // Retrieves all entries with the given |download_namespace|.
  virtual void LoadEntries(LoadEntriesCallback callback) = 0;

  // Removes the Entry associated with |guid| from the storage.
  virtual void Remove(const std::string& guid) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_H_
