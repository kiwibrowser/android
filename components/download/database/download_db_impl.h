// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_IMPL_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_IMPL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/database/download_db.h"
#include "components/leveldb_proto/proto_database.h"

namespace download_pb {
class DownloadDBEntry;
}

namespace download {

// A protodb Implementation of DownloadDB.
class DownloadDBImpl : public DownloadDB {
 public:
  DownloadDBImpl(DownloadNamespace download_namespace,
                 const base::FilePath& database_dir);
  DownloadDBImpl(
      DownloadNamespace download_namespace,
      const base::FilePath& database_dir,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<download_pb::DownloadDBEntry>> db);
  ~DownloadDBImpl() override;

  // DownloadDB implementation.
  bool IsInitialized() override;
  void Initialize(InitializeCallback callback) override;
  void AddOrReplace(const DownloadDBEntry& entry) override;
  void LoadEntries(LoadEntriesCallback callback) override;
  void Remove(const std::string& guid) override;

 private:
  friend class DownloadDBTest;

  void DestroyAndReinitialize(InitializeCallback callback);

  // Returns the key of the db entry.
  std::string GetEntryKey(const std::string& guid) const;

  // Called when database is initialized.
  void OnDatabaseInitialized(InitializeCallback callback, bool success);

  // Called when database is destroyed.
  void OnDatabaseDestroyed(InitializeCallback callback, bool success);

  // Called when entry is updated.
  void OnUpdateDone(bool success);

  // Called when entry is removed.
  void OnRemoveDone(bool success);

  // Called when all database entries are loaded.
  void OnAllEntriesLoaded(
      LoadEntriesCallback callback,
      bool success,
      std::unique_ptr<std::vector<download_pb::DownloadDBEntry>> entries);

  // Directory to store |db_|.
  base::FilePath database_dir_;

  // Proto db for storing all the entries.
  std::unique_ptr<leveldb_proto::ProtoDatabase<download_pb::DownloadDBEntry>>
      db_;

  // Whether the object is initialized.
  bool is_initialized_;

  // Namespace of this db.
  DownloadNamespace download_namespace_;

  base::WeakPtrFactory<DownloadDBImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDBImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_DOWNLOAD_DB_IMPL_H_
