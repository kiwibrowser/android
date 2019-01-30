// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_storage_database.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "components/feed/core/proto/feed_storage.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feed {

namespace {
using StorageEntryVector =
    leveldb_proto::ProtoDatabase<FeedStorageProto>::KeyEntryVector;

// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.FeedStorageDatabase. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kStorageDatabaseUMAClientName[] = "FeedStorageDatabase";

const char kStorageDatabaseFolder[] = "storage";

const size_t kDatabaseWriteBufferSizeBytes = 512 * 1024;
const size_t kDatabaseWriteBufferSizeBytesForLowEndDevice = 128 * 1024;

// Key prefixes for content's storage key and journal's storage key. Because we
// put both content data and journal data into one storage, we need to add
// prefixes to their keys to distinguish between content keys and journal keys.
const char kContentStoragePrefix[] = "cs-";
const char kJournalStoragePrefix[] = "js-";

// Formats content key to storage key by adding a prefix.
std::string FormatContentKeyToStorageKey(const std::string& content_key) {
  return kContentStoragePrefix + content_key;
}

// Formats journal key to storage key by adding a prefix.
std::string FormatJournalKeyToStorageKey(const std::string& journal_key) {
  return kJournalStoragePrefix + journal_key;
}

// Check if the |storage_key| is for journal data.
bool IsValidJournalKey(const std::string& storage_key) {
  return base::StartsWith(storage_key, kJournalStoragePrefix,
                          base::CompareCase::SENSITIVE);
}

// Parse journal key from storage key. Return an empty string if |storage_key|
// is not recognized as journal key. ex. content's storage key.
std::string ParseJournalKey(const std::string& storage_key) {
  if (!IsValidJournalKey(storage_key)) {
    return std::string();
  }

  return storage_key.substr(strlen(kJournalStoragePrefix));
}

bool DatabaseKeyFilter(const std::unordered_set<std::string>& key_set,
                       const std::string& key) {
  return key_set.find(key) != key_set.end();
}

bool DatabasePrefixFilter(const std::string& key_prefix,
                          const std::string& key) {
  return base::StartsWith(key, key_prefix, base::CompareCase::SENSITIVE);
}

}  // namespace

FeedStorageDatabase::FeedStorageDatabase(
    const base::FilePath& database_folder,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : FeedStorageDatabase(
          database_folder,
          std::make_unique<leveldb_proto::ProtoDatabaseImpl<FeedStorageProto>>(
              task_runner)) {}

FeedStorageDatabase::FeedStorageDatabase(
    const base::FilePath& database_folder,
    std::unique_ptr<leveldb_proto::ProtoDatabase<FeedStorageProto>>
        storage_database)
    : database_status_(UNINITIALIZED),
      storage_database_(std::move(storage_database)),
      weak_ptr_factory_(this) {
  leveldb_env::Options options = leveldb_proto::CreateSimpleOptions();
  if (base::SysInfo::IsLowEndDevice()) {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytesForLowEndDevice;
  } else {
    options.write_buffer_size = kDatabaseWriteBufferSizeBytes;
  }

  base::FilePath storage_folder =
      database_folder.AppendASCII(kStorageDatabaseFolder);
  storage_database_->Init(
      kStorageDatabaseUMAClientName, storage_folder, options,
      base::BindOnce(&FeedStorageDatabase::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

FeedStorageDatabase::~FeedStorageDatabase() = default;

bool FeedStorageDatabase::IsInitialized() const {
  return INITIALIZED == database_status_;
}

void FeedStorageDatabase::LoadContent(const std::vector<std::string>& keys,
                                      ContentLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unordered_set<std::string> key_set;
  for (const auto& key : keys) {
    key_set.insert(FormatContentKeyToStorageKey(key));
  }

  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabaseKeyFilter, std::move(key_set)),
      base::BindOnce(&FeedStorageDatabase::OnLoadEntriesForLoadContent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::LoadContentByPrefix(const std::string& prefix,
                                              ContentLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key_prefix = FormatContentKeyToStorageKey(prefix);

  storage_database_->LoadEntriesWithFilter(
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedStorageDatabase::OnLoadEntriesForLoadContent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::SaveContent(std::vector<KeyAndData> pairs,
                                      ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto contents_to_save = std::make_unique<StorageEntryVector>();
  for (auto entry : pairs) {
    FeedStorageProto proto;
    proto.set_key(std::move(entry.first));
    proto.set_content_data(std::move(entry.second));
    contents_to_save->emplace_back(FormatContentKeyToStorageKey(proto.key()),
                                   std::move(proto));
  }

  storage_database_->UpdateEntries(
      std::move(contents_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::DeleteContent(
    const std::vector<std::string>& keys_to_delete,
    ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto content_to_delete = std::make_unique<std::vector<std::string>>();
  for (const auto& key : keys_to_delete) {
    content_to_delete->emplace_back(FormatContentKeyToStorageKey(key));
  }
  storage_database_->UpdateEntries(
      std::make_unique<StorageEntryVector>(), std::move(content_to_delete),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::DeleteContentByPrefix(
    const std::string& prefix_to_delete,
    ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key_prefix = FormatContentKeyToStorageKey(prefix_to_delete);
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(),
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::DeleteAllContent(ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key_prefix = FormatContentKeyToStorageKey(std::string());
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(),
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::LoadJournal(const std::string& key,
                                      JournalLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->GetEntry(
      FormatJournalKeyToStorageKey(key),
      base::BindOnce(&FeedStorageDatabase::OnGetEntryForLoadJournal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::LoadAllJournalKeys(JournalLoadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->LoadKeys(
      base::BindOnce(&FeedStorageDatabase::OnLoadKeysForLoadAllJournalKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::AppendToJournal(const std::string& key,
                                          std::vector<std::string> entries,
                                          ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->GetEntry(
      FormatJournalKeyToStorageKey(key),
      base::BindOnce(&FeedStorageDatabase::OnGetEntryAppendToJournal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback), key,
                     std::move(entries)));
}

void FeedStorageDatabase::CopyJournal(const std::string& from_key,
                                      const std::string& to_key,
                                      ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  storage_database_->GetEntry(
      FormatJournalKeyToStorageKey(from_key),
      base::BindOnce(&FeedStorageDatabase::OnGetEntryForCopyJournal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     to_key));
}

void FeedStorageDatabase::DeleteJournal(const std::string& key,
                                        ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto journals_to_delete = std::make_unique<std::vector<std::string>>();
  journals_to_delete->push_back(FormatJournalKeyToStorageKey(key));

  storage_database_->UpdateEntries(
      std::make_unique<StorageEntryVector>(), std::move(journals_to_delete),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::DeleteAllJournals(ConfirmationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string key_prefix = FormatJournalKeyToStorageKey(std::string());
  storage_database_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StorageEntryVector>(),
      base::BindRepeating(&DatabasePrefixFilter, std::move(key_prefix)),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::OnDatabaseInitialized(bool success) {
  DCHECK_EQ(database_status_, UNINITIALIZED);

  if (success) {
    database_status_ = INITIALIZED;
  } else {
    database_status_ = INIT_FAILURE;
    DVLOG(1) << "FeedStorageDatabase init failed.";
  }
}

void FeedStorageDatabase::OnLoadEntriesForLoadContent(
    ContentLoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<FeedStorageProto>> content) {
  std::vector<KeyAndData> results;

  if (!success || !content) {
    DVLOG_IF(1, !success) << "FeedStorageDatabase load content failed.";
    std::move(callback).Run(std::move(results));
    return;
  }

  for (const auto& proto : *content) {
    DCHECK(proto.has_key());
    DCHECK(proto.has_content_data());

    results.emplace_back(proto.key(), proto.content_data());
  }

  std::move(callback).Run(std::move(results));
}

void FeedStorageDatabase::OnGetEntryForLoadJournal(
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<FeedStorageProto> journal) {
  std::vector<std::string> results;

  if (!success || !journal) {
    DVLOG_IF(1, !success) << "FeedStorageDatabase load journal failed.";
    std::move(callback).Run(std::move(results));
    return;
  }

  for (int i = 0; i < journal->journal_data_size(); ++i) {
    results.emplace_back(journal->journal_data(i));
  }

  std::move(callback).Run(std::move(results));
}

void FeedStorageDatabase::OnGetEntryAppendToJournal(
    ConfirmationCallback callback,
    std::string key,
    std::vector<std::string> entries,
    bool success,
    std::unique_ptr<FeedStorageProto> journal) {
  if (!success) {
    DVLOG(1) << "FeedStorageDatabase load journal failed.";
    std::move(callback).Run(success);
    return;
  }

  if (journal == nullptr) {
    // The journal does not exist, create a new one.
    journal = std::make_unique<FeedStorageProto>();
    journal->set_key(key);
  }
  DCHECK_EQ(journal->key(), key);

  for (const std::string& entry : entries) {
    journal->add_journal_data(entry);
  }
  auto journals_to_save = std::make_unique<StorageEntryVector>();
  journals_to_save->emplace_back(FormatJournalKeyToStorageKey(key),
                                 std::move(*journal));

  storage_database_->UpdateEntries(
      std::move(journals_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::OnGetEntryForCopyJournal(
    ConfirmationCallback callback,
    std::string to_key,
    bool success,
    std::unique_ptr<FeedStorageProto> journal) {
  if (!success || !journal) {
    DVLOG_IF(1, !success) << "FeedStorageDatabase load journal failed.";
    std::move(callback).Run(success);
    return;
  }

  journal->set_key(to_key);
  auto journal_to_save = std::make_unique<StorageEntryVector>();
  journal_to_save->emplace_back(FormatJournalKeyToStorageKey(to_key),
                                std::move(*journal));

  storage_database_->UpdateEntries(
      std::move(journal_to_save), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&FeedStorageDatabase::OnStorageCommitted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FeedStorageDatabase::OnLoadKeysForLoadAllJournalKeys(
    JournalLoadCallback callback,
    bool success,
    std::unique_ptr<std::vector<std::string>> keys) {
  std::vector<std::string> results;

  if (!success || !keys) {
    DVLOG_IF(1, !success) << "FeedStorageDatabase load journal keys failed.";
    std::move(callback).Run(std::move(results));
    return;
  }

  // Filter out content keys, only keep journal keys.
  for (const std::string& key : *keys) {
    if (IsValidJournalKey(key))
      results.emplace_back(ParseJournalKey(key));
  }

  std::move(callback).Run(std::move(results));
}

void FeedStorageDatabase::OnStorageCommitted(ConfirmationCallback callback,
                                             bool success) {
  DVLOG_IF(1, !success) << "FeedStorageDatabase committed failed.";
  std::move(callback).Run(success);
}

}  // namespace feed
