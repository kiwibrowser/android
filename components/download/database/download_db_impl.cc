// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/database/download_db_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/task_scheduler/post_task.h"
#include "components/download/database/download_db_conversions.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/database/proto/download_entry.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace download {

namespace {

const char kDatabaseClientName[] = "DownloadDB";
using ProtoKeyVector = std::vector<std::string>;
using ProtoEntryVector = std::vector<download_pb::DownloadDBEntry>;
using ProtoKeyEntryVector =
    std::vector<std::pair<std::string, download_pb::DownloadDBEntry>>;

// Returns the prefix to all keys in the database.
std::string GetDatabaseKeyPrefix(DownloadNamespace download_namespace) {
  return DownloadNamespaceToString(download_namespace) + ",";
}

// Check if an input string is under a given namespace.
bool IsUnderNameSpace(DownloadNamespace download_namespace,
                      const std::string& key) {
  return base::StartsWith(key, GetDatabaseKeyPrefix(download_namespace),
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace

DownloadDBImpl::DownloadDBImpl(DownloadNamespace download_namespace,
                               const base::FilePath& database_dir)
    : DownloadDBImpl(
          download_namespace,
          database_dir,
          std::make_unique<
              leveldb_proto::ProtoDatabaseImpl<download_pb::DownloadDBEntry>>(
              base::CreateSequencedTaskRunnerWithTraits(
                  {base::MayBlock(), base::TaskPriority::BACKGROUND,
                   base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))) {}

DownloadDBImpl::DownloadDBImpl(
    DownloadNamespace download_namespace,
    const base::FilePath& database_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<download_pb::DownloadDBEntry>>
        db)
    : database_dir_(database_dir),
      db_(std::move(db)),
      is_initialized_(false),
      download_namespace_(download_namespace),
      weak_factory_(this) {}

DownloadDBImpl::~DownloadDBImpl() = default;

bool DownloadDBImpl::IsInitialized() {
  return is_initialized_;
}

void DownloadDBImpl::Initialize(InitializeCallback callback) {
  DCHECK(!IsInitialized());
  // These options reduce memory consumption.
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  options.reuse_logs = false;
  options.write_buffer_size = 64 << 10;  // 64 KiB
  db_->Init(kDatabaseClientName, database_dir_, options,
            base::BindOnce(&DownloadDBImpl::OnDatabaseInitialized,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
  // TODO(qinmin): migrate all the data from InProgressCache into this database.
}

void DownloadDBImpl::DestroyAndReinitialize(InitializeCallback callback) {
  is_initialized_ = false;
  db_->Destroy(base::BindOnce(&DownloadDBImpl::OnDatabaseDestroyed,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadDBImpl::AddOrReplace(const DownloadDBEntry& entry) {
  DCHECK(IsInitialized());
  auto entries_to_save = std::make_unique<ProtoKeyEntryVector>();
  download_pb::DownloadDBEntry proto =
      DownloadDBConversions::DownloadDBEntryToProto(entry);
  entries_to_save->emplace_back(GetEntryKey(entry.GetGuid()), std::move(proto));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<ProtoKeyVector>(),
                     base::BindOnce(&DownloadDBImpl::OnUpdateDone,
                                    weak_factory_.GetWeakPtr()));
}

void DownloadDBImpl::LoadEntries(LoadEntriesCallback callback) {
  db_->LoadEntriesWithFilter(
      base::BindRepeating(&IsUnderNameSpace, download_namespace_),
      base::BindOnce(&DownloadDBImpl::OnAllEntriesLoaded,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void DownloadDBImpl::Remove(const std::string& guid) {
  DCHECK(IsInitialized());
  auto keys_to_remove = std::make_unique<ProtoKeyVector>();
  keys_to_remove->push_back(GetEntryKey(guid));
  db_->UpdateEntries(std::make_unique<ProtoKeyEntryVector>(),
                     std::move(keys_to_remove),
                     base::BindOnce(&DownloadDBImpl::OnRemoveDone,
                                    weak_factory_.GetWeakPtr()));
}

std::string DownloadDBImpl::GetEntryKey(const std::string& guid) const {
  return GetDatabaseKeyPrefix(download_namespace_) + guid;
}

void DownloadDBImpl::OnAllEntriesLoaded(
    LoadEntriesCallback callback,
    bool success,
    std::unique_ptr<ProtoEntryVector> entries) {
  auto result = std::make_unique<std::vector<DownloadDBEntry>>();
  if (!success) {
    std::move(callback).Run(success, std::move(result));
    return;
  }

  for (const auto& entry : *entries.get()) {
    result->emplace_back(
        DownloadDBConversions::DownloadDBEntryFromProto(entry));
  }
  std::move(callback).Run(success, std::move(result));
}

void DownloadDBImpl::OnDatabaseInitialized(InitializeCallback callback,
                                           bool success) {
  is_initialized_ = success;
  std::move(callback).Run(success);
}

void DownloadDBImpl::OnDatabaseDestroyed(InitializeCallback callback,
                                         bool success) {
  if (!success) {
    std::move(callback).Run(success);
    return;
  }

  Initialize(std::move(callback));
}

void DownloadDBImpl::OnUpdateDone(bool success) {
  // TODO(qinmin): add UMA for this.
  if (!success)
    LOG(ERROR) << "Update Download DB failed.";
}

void DownloadDBImpl::OnRemoveDone(bool success) {
  // TODO(qinmin): add UMA for this.
  if (!success)
    LOG(ERROR) << "Remove entry from Download DB failed.";
}

}  // namespace download
