// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_storage_database.h"

#include <map>

#include "base/test/scoped_task_environment.h"
#include "components/feed/core/proto/feed_storage.pb.h"
#include "components/feed/core/time_serialization.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;
using testing::NotNull;
using testing::_;

namespace feed {

namespace {
const std::string kContentKeyPrefix = "ContentKey";
const std::string kContentKey1 = "ContentKey1";
const std::string kContentKey2 = "ContentKey2";
const std::string kContentKey3 = "ContentKey3";
const std::string kContentData1 = "Content Data1";
const std::string kContentData2 = "Content Data2";
const std::string kContentData3 = "Content Data3";
const std::string kJournalKey1 = "JournalKey1";
const std::string kJournalKey2 = "JournalKey2";
const std::string kJournalKey3 = "JournalKey3";
const std::string kJournalData1 = "Journal Data1";
const std::string kJournalData2 = "Journal Data2";
const std::string kJournalData3 = "Journal Data3";
const std::string kJournalData4 = "Journal Data4";
const std::string kJournalData5 = "Journal Data5";
const std::string kJournalData6 = "Journal Data6";
}  // namespace

class FeedStorageDatabaseTest : public testing::Test {
 public:
  FeedStorageDatabaseTest() : storage_db_(nullptr) {}

  void CreateDatabase(bool init_database) {
    // The FakeDBs are owned by |feed_db_|, so clear our pointers before
    // resetting |feed_db_| itself.
    storage_db_ = nullptr;
    // Explicitly destroy any existing database before creating a new one.
    feed_db_.reset();

    auto storage_db =
        std::make_unique<FakeDB<FeedStorageProto>>(&storage_db_storage_);

    storage_db_ = storage_db.get();
    feed_db_ = std::make_unique<FeedStorageDatabase>(base::FilePath(),
                                                     std::move(storage_db));
    if (init_database) {
      storage_db_->InitCallback(true);
      ASSERT_TRUE(db()->IsInitialized());
    }
  }

  void InjectContentStorageProto(const std::string& key,
                                 const std::string& data) {
    FeedStorageProto storage_proto;
    storage_proto.set_key(key);
    storage_proto.set_content_data(data);
    storage_db_storage_["cs-" + key] = storage_proto;
  }

  void InjectJournalStorageProto(const std::string& key,
                                 const std::vector<std::string>& entries) {
    FeedStorageProto storage_proto;
    storage_proto.set_key(key);
    for (const std::string& entry : entries) {
      storage_proto.add_journal_data(entry);
    }
    storage_db_storage_["js-" + key] = storage_proto;
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  FakeDB<FeedStorageProto>* storage_db() { return storage_db_; }

  FeedStorageDatabase* db() { return feed_db_.get(); }

  MOCK_METHOD1(OnContentEntriesReceived,
               void(std::vector<std::pair<std::string, std::string>>));
  MOCK_METHOD1(OnJournalEntryReceived, void(std::vector<std::string>));
  MOCK_METHOD1(OnStorageCommitted, void(bool));

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::map<std::string, FeedStorageProto> storage_db_storage_;

  // Owned by |feed_db_|.
  FakeDB<FeedStorageProto>* storage_db_;

  std::unique_ptr<FeedStorageDatabase> feed_db_;

  DISALLOW_COPY_AND_ASSIGN(FeedStorageDatabaseTest);
};

TEST_F(FeedStorageDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase(/*init_database=*/false);

  storage_db()->InitCallback(true);
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(FeedStorageDatabaseTest, LoadContentAfterInitSuccess) {
  CreateDatabase(/*init_database=*/true);

  EXPECT_CALL(*this, OnContentEntriesReceived(_));
  db()->LoadContent(
      {kContentKey1},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedStorageDatabaseTest, LoadContentsEntries) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  // Try to Load |kContentKey2| and |kContentKey3|, only |kContentKey2| should
  // return.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey2);
        EXPECT_EQ(results[0].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey2, kContentKey3},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedStorageDatabaseTest, LoadContentsEntriesByPrefix) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|, |kJournalKey1|, |kJournalKey2|,
  // |kJournalKey3|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  // Try to Load "ContentKey", both |kContentKey1| and |kContentKey2| should
  // return.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContentByPrefix(
      kContentKeyPrefix,
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedStorageDatabaseTest, SaveContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|, |kJournalKey1|, |kJournalKey2|,
  // |kJournalKey3|.
  std::vector<std::pair<std::string, std::string>> entries;
  entries.push_back(std::make_pair(kContentKey1, kContentData1));
  entries.push_back(std::make_pair(kContentKey2, kContentData2));
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->SaveContent(std::move(entries),
                    base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                                   base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure they're there.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedStorageDatabaseTest, DeleteContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete |kContentKey2| and |kContentKey3|
  std::vector<std::string> keys;
  keys.push_back(kContentKey2);
  keys.push_back(kContentKey3);
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->DeleteContent(
      std::move(keys),
      base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure only |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedStorageDatabaseTest, DeleteContentByPrefix) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Delete |kContentKey1| and |kContentKey2|
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->DeleteContentByPrefix(
      kContentKeyPrefix,
      base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure |kContentKey1| and |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 0U);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

TEST_F(FeedStorageDatabaseTest, DeleteAllContent) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Store |kJournalKey1|, |kJournalKey2|, |kJournalKey3|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  // Delete all content, meaning |kContentKey1| and |kContentKey2| are expected
  // to be deleted.
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->DeleteAllContent(base::BindOnce(
      &FeedStorageDatabaseTest::OnStorageCommitted, base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure |kContentKey1| and |kContentKey2| got deleted.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        EXPECT_EQ(results.size(), 0U);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);

  // Make sure all journals are there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->LoadKeysCallback(true);
}

TEST_F(FeedStorageDatabaseTest, LoadJournalEntry) {
  CreateDatabase(/*init_database=*/true);

  // Store |kJournalKey1|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

TEST_F(FeedStorageDatabaseTest, LoadNonExistingJournalEntry) {
  CreateDatabase(/*init_database=*/true);

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 0U);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

TEST_F(FeedStorageDatabaseTest, LoadAllJournalKeys) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1|, |kContentKey2|, |kJournalKey1|, |kJournalKey2|,
  // |kJournalKey3|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalKey1);
        EXPECT_EQ(results[1], kJournalKey2);
        EXPECT_EQ(results[2], kJournalKey3);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->LoadKeysCallback(true);
}

TEST_F(FeedStorageDatabaseTest, AppendToJournal_WhenJournalExists) {
  CreateDatabase(/*init_database=*/true);

  // Save |kContentKey1|
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->AppendToJournal(
      kJournalKey1, {kJournalData1, kJournalData2},
      base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
  storage_db()->UpdateCallback(true);

  // Make sure they're there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);

  Mock::VerifyAndClearExpectations(this);

  // Append more for |kContentKey1|
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->AppendToJournal(
      kJournalKey1, {kJournalData3, kJournalData4, kJournalData5},
      base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
  storage_db()->UpdateCallback(true);

  // Check new instances are there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 5U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
        EXPECT_EQ(results[3], kJournalData4);
        EXPECT_EQ(results[4], kJournalData5);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

TEST_F(FeedStorageDatabaseTest, AppendToJournal_WhenJournalMissing) {
  CreateDatabase(/*init_database=*/true);

  // Append data for |kContentKey1|
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->AppendToJournal(
      kJournalKey1, {kJournalData1, kJournalData2, kJournalData3},
      base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
  storage_db()->UpdateCallback(true);

  // Check new data are there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

TEST_F(FeedStorageDatabaseTest, CopyJournal) {
  CreateDatabase(/*init_database=*/true);

  // Save |kContentKey1|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});

  // Copy |kContentKey1| to |kContentKey2|.
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->CopyJournal(kJournalKey1, kJournalKey2,
                    base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                                   base::Unretained(this)));
  storage_db()->GetCallback(true);
  storage_db()->UpdateCallback(true);

  // Check new journal is there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey2,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);

  Mock::VerifyAndClearExpectations(this);

  // Check first journal is still there.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

TEST_F(FeedStorageDatabaseTest, DeleteJournal) {
  CreateDatabase(/*init_database=*/true);

  // Store |kJournalKey1|, |kJournalKey2|, |kJournalKey3|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  // Delete |kJournalKey2|.
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->DeleteJournal(
      kJournalKey2, base::BindOnce(&FeedStorageDatabaseTest::OnStorageCommitted,
                                   base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure |kJournalKey2| got deleted.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0], kJournalKey1);
        EXPECT_EQ(results[1], kJournalKey3);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->LoadKeysCallback(true);
}

TEST_F(FeedStorageDatabaseTest, DeleteAllJournals) {
  CreateDatabase(/*init_database=*/true);

  // Store |kContentKey1| and |kContentKey2|.
  InjectContentStorageProto(kContentKey1, kContentData1);
  InjectContentStorageProto(kContentKey2, kContentData2);

  // Store |kJournalKey1|, |kJournalKey2|, |kJournalKey3|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4, kJournalData5});
  InjectJournalStorageProto(kJournalKey3, {kJournalData6});

  // Delete all journals, meaning |kJournalKey1|, |kJournalKey2| and
  // |kJournalKey3| are expected to be deleted.
  EXPECT_CALL(*this, OnStorageCommitted(true));
  db()->DeleteAllJournals(base::BindOnce(
      &FeedStorageDatabaseTest::OnStorageCommitted, base::Unretained(this)));
  storage_db()->UpdateCallback(true);

  // Make sure all journals got deleted.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 0U);
      });
  db()->LoadAllJournalKeys(
      base::BindOnce(&FeedStorageDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->LoadKeysCallback(true);

  // Make sure all content are still there.
  EXPECT_CALL(*this, OnContentEntriesReceived(_))
      .WillOnce([](std::vector<std::pair<std::string, std::string>> results) {
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].first, kContentKey1);
        EXPECT_EQ(results[0].second, kContentData1);
        EXPECT_EQ(results[1].first, kContentKey2);
        EXPECT_EQ(results[1].second, kContentData2);
      });
  db()->LoadContent(
      {kContentKey1, kContentKey2},
      base::BindOnce(&FeedStorageDatabaseTest::OnContentEntriesReceived,
                     base::Unretained(this)));
  storage_db()->LoadCallback(true);
}

}  // namespace feed
