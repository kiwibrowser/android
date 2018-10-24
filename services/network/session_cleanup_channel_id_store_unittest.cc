// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/session_cleanup_channel_id_store.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/test/channel_id_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using ChannelIDVector =
    std::vector<std::unique_ptr<net::DefaultChannelIDStore::ChannelID>>;

const base::FilePath::CharType kTestChannelIDFilename[] =
    FILE_PATH_LITERAL("ChannelID");

class SessionCleanupChannelIDStoreTest : public testing::Test {
 public:
  ChannelIDVector Load() {
    ChannelIDVector channel_ids;
    base::RunLoop run_loop;
    store_->Load(
        base::BindRepeating(&SessionCleanupChannelIDStoreTest::OnLoaded,
                            base::Unretained(this), &run_loop, &channel_ids));
    run_loop.Run();
    return channel_ids;
  }

  void OnLoaded(base::RunLoop* run_loop,
                ChannelIDVector* channel_ids_out,
                std::unique_ptr<ChannelIDVector> channel_ids) {
    channel_ids_out->swap(*channel_ids);
    run_loop->Quit();
  }

  ChannelIDVector CreateAndLoad() {
    store_ = base::MakeRefCounted<SessionCleanupChannelIDStore>(
        temp_dir_.GetPath().Append(kTestChannelIDFilename),
        scoped_task_environment_.GetMainThreadTaskRunner());
    return Load();
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ChannelIDVector channel_ids = CreateAndLoad();
    ASSERT_EQ(0u, channel_ids.size());
  }

  void TearDown() override {
    store_ = nullptr;
    scoped_task_environment_.RunUntilIdle();
  }

  base::ScopedTempDir temp_dir_;
  scoped_refptr<SessionCleanupChannelIDStore> store_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(SessionCleanupChannelIDStoreTest, TestPersistence) {
  std::unique_ptr<crypto::ECPrivateKey> goog_key(
      crypto::ECPrivateKey::Create());
  std::unique_ptr<crypto::ECPrivateKey> foo_key(crypto::ECPrivateKey::Create());
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "google.com", base::Time::FromDoubleT(1), goog_key->Copy()));
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "foo.com", base::Time::FromDoubleT(3), foo_key->Copy()));

  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk. Then we can see if after loading it again it
  // is still there.
  store_ = nullptr;
  // Make sure we wait until the destructor has run.
  scoped_task_environment_.RunUntilIdle();

  // Reload and test for persistence
  ChannelIDVector channel_ids = CreateAndLoad();
  ASSERT_EQ(2u, channel_ids.size());
  net::DefaultChannelIDStore::ChannelID* goog_channel_id;
  net::DefaultChannelIDStore::ChannelID* foo_channel_id;
  if (channel_ids[0]->server_identifier() == "google.com") {
    goog_channel_id = channel_ids[0].get();
    foo_channel_id = channel_ids[1].get();
  } else {
    goog_channel_id = channel_ids[1].get();
    foo_channel_id = channel_ids[0].get();
  }
  EXPECT_EQ("google.com", goog_channel_id->server_identifier());
  EXPECT_TRUE(net::KeysEqual(goog_key.get(), goog_channel_id->key()));
  EXPECT_EQ(1, goog_channel_id->creation_time().ToDoubleT());
  EXPECT_EQ("foo.com", foo_channel_id->server_identifier());
  EXPECT_TRUE(net::KeysEqual(foo_key.get(), foo_channel_id->key()));
  EXPECT_EQ(3, foo_channel_id->creation_time().ToDoubleT());

  // Now delete the channel ID and check persistence again.
  store_->DeleteChannelID(*channel_ids[0]);
  store_->DeleteChannelID(*channel_ids[1]);
  store_ = nullptr;
  // Make sure we wait until the destructor has run.
  scoped_task_environment_.RunUntilIdle();

  // Reload and check if the channel ID has been removed.
  channel_ids = CreateAndLoad();
  EXPECT_EQ(0u, channel_ids.size());
}

TEST_F(SessionCleanupChannelIDStoreTest, TestDeleteSessionChannelIDs) {
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "google.com", base::Time::FromDoubleT(1),
      crypto::ECPrivateKey::Create()));
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "nonpersistent.com", base::Time::FromDoubleT(3),
      crypto::ECPrivateKey::Create()));

  // Replace the store and force it to write to disk.
  store_ = nullptr;
  scoped_task_environment_.RunUntilIdle();
  ChannelIDVector channel_ids = CreateAndLoad();
  EXPECT_EQ(2u, channel_ids.size());

  // Add another two channel IDs before closing the store. Because additions are
  // delayed and committed to disk in batches, these will not be committed until
  // the store is destroyed, which is after the policy is applied. The pending
  // operation pruning logic should prevent the "nonpersistent.com" ID from
  // being committed to disk.
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "nonpersistent.com", base::Time::FromDoubleT(5),
      crypto::ECPrivateKey::Create()));
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "persistent.com", base::Time::FromDoubleT(7),
      crypto::ECPrivateKey::Create()));

  store_->DeleteSessionChannelIDs(base::BindRepeating(
      [](const GURL& url) { return url.host() == "nonpersistent.com"; }));
  scoped_task_environment_.RunUntilIdle();
  // Now close the store, and the nonpersistent.com channel IDs should have been
  // deleted.
  store_ = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Reload and check that the nonpersistent.com channel IDs have been removed.
  channel_ids = CreateAndLoad();
  EXPECT_EQ(2u, channel_ids.size());
  for (const auto& id : channel_ids) {
    EXPECT_NE("nonpersistent.com", id->server_identifier());
  }
}

TEST_F(SessionCleanupChannelIDStoreTest, TestForceKeepSessionState) {
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "google.com", base::Time::FromDoubleT(1),
      crypto::ECPrivateKey::Create()));
  store_->AddChannelID(net::DefaultChannelIDStore::ChannelID(
      "nonpersistent.com", base::Time::FromDoubleT(3),
      crypto::ECPrivateKey::Create()));

  store_->SetForceKeepSessionState();
  store_->DeleteSessionChannelIDs(base::BindRepeating(
      [](const GURL& url) { return url.host() == "nonpersistent.com"; }));
  scoped_task_environment_.RunUntilIdle();

  store_ = nullptr;
  scoped_task_environment_.RunUntilIdle();

  // Reload and check that the all channel IDs are still present.
  ChannelIDVector channel_ids = CreateAndLoad();
  EXPECT_EQ(2u, channel_ids.size());
}

}  // namespace
}  // namespace network
