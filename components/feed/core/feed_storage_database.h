// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_STORAGE_DATABASE_H_
#define COMPONENTS_FEED_CORE_FEED_STORAGE_DATABASE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database.h"

namespace feed {

class FeedStorageProto;

// FeedStorageDatabase is leveldb backed store for feed's content storage data
// and jounal storage data.
class FeedStorageDatabase {
 public:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    INIT_FAILURE,
  };

  using KeyAndData = std::pair<std::string, std::string>;

  // Returns the storage data as a vector of key-value pairs when calling
  // loading data.
  using ContentLoadCallback = base::OnceCallback<void(std::vector<KeyAndData>)>;

  // Returns the journal data as a vector of strings when calling loading data.
  using JournalLoadCallback =
      base::OnceCallback<void(std::vector<std::string>)>;

  // Returns whether the commit operation succeeded.
  using ConfirmationCallback = base::OnceCallback<void(bool)>;

  // Initializes the database with |database_folder|.
  FeedStorageDatabase(
      const base::FilePath& database_folder,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Initializes the database with |database_folder|. Creates storage using the
  // given |storage_database| for local storage. Useful for testing.
  FeedStorageDatabase(
      const base::FilePath& database_folder,
      std::unique_ptr<leveldb_proto::ProtoDatabase<FeedStorageProto>>
          storage_database);

  ~FeedStorageDatabase();

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may already started, or initialization
  // failed.
  bool IsInitialized() const;

  // Loads the content data for the |keys| and passes them to |callback|.
  void LoadContent(const std::vector<std::string>& keys,
                   ContentLoadCallback callback);

  // Loads the content data whose key matches |prefix|, and passes them to
  // |callback|.
  void LoadContentByPrefix(const std::string& prefix,
                           ContentLoadCallback callback);

  // Inserts or updates the content data |pairs|, |callback| will be called when
  // the data are saved or if there is an error. The fields in |pairs| will be
  // std::move.
  void SaveContent(std::vector<KeyAndData> pairs,
                   ConfirmationCallback callback);

  // Deletes the content data for |keys_to_delete|, |callback| will be called
  // when the data are deleted or if there is an error.
  void DeleteContent(const std::vector<std::string>& keys_to_delete,
                     ConfirmationCallback callback);

  // Deletes the content data whose key matches |prefix_to_delete|, |callback|
  // will be called when the content are deleted or if there is an error.
  void DeleteContentByPrefix(const std::string& prefix_to_delete,
                             ConfirmationCallback callback);

  // Delete all content, |callback| will be called when all content is deleted
  // or if there is an error.
  void DeleteAllContent(ConfirmationCallback callback);

  // Loads the journal data for the |key| and passes it to |callback|.
  void LoadJournal(const std::string& key, JournalLoadCallback callback);

  // Loads all journal keys in the storage, and passes them to |callback|.
  void LoadAllJournalKeys(JournalLoadCallback callback);

  // Appends |entries| to a journal whose key is |key|, if there the journal do
  // not exist, create one. |callback| will be called when the data are saved or
  // if there is an error.
  void AppendToJournal(const std::string& key,
                       std::vector<std::string> entries,
                       ConfirmationCallback callback);

  // Creates a new journal with name |to_key|, and copys all data from the
  // journal with |from_key| to it. |callback| will be called when the data are
  // saved or if there is an error.
  void CopyJournal(const std::string& from_key,
                   const std::string& to_key,
                   ConfirmationCallback callback);

  // Deletes the journal with |key|, |callback| will be called when the journal
  // is deleted or if there is an error.
  void DeleteJournal(const std::string& key, ConfirmationCallback callback);

  // Delete all journals, |callback| will be called when all journals are
  // deleted or if there is an error.
  void DeleteAllJournals(ConfirmationCallback callback);

 private:
  // Callback methods given to |storage_database_| for async responses.
  void OnDatabaseInitialized(bool success);
  void OnLoadEntriesForLoadContent(
      ContentLoadCallback callback,
      bool success,
      std::unique_ptr<std::vector<FeedStorageProto>> content);
  void OnGetEntryForLoadJournal(JournalLoadCallback callback,
                                bool success,
                                std::unique_ptr<FeedStorageProto> journal);
  void OnGetEntryAppendToJournal(ConfirmationCallback callback,
                                 std::string key,
                                 std::vector<std::string> entries,
                                 bool success,
                                 std::unique_ptr<FeedStorageProto> journal);
  void OnGetEntryForCopyJournal(ConfirmationCallback callback,
                                std::string to_key,
                                bool success,
                                std::unique_ptr<FeedStorageProto> journal);
  void OnLoadKeysForLoadAllJournalKeys(
      JournalLoadCallback callback,
      bool success,
      std::unique_ptr<std::vector<std::string>> keys);
  void OnStorageCommitted(ConfirmationCallback callback, bool success);

  State database_status_;

  std::unique_ptr<leveldb_proto::ProtoDatabase<FeedStorageProto>>
      storage_database_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FeedStorageDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedStorageDatabase);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_STORAGE_DATABASE_H_
