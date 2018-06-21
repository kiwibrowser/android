// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker.h"

#include "base/base64.h"
#include "base/sha1.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/time.h"
#include "components/sync/model/entity_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::IsNull;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& title,
                                           const std::string& url) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_bookmark()->set_title(title);
  specifics.mutable_bookmark()->set_url(url);
  return specifics;
}

TEST(SyncedBookmarkTrackerTest, ShouldGetAssociatedNodes) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.foo.com");
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kCreationTime(base::Time::Now() -
                                 base::TimeDelta::FromSeconds(1));
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());

  bookmarks::BookmarkNode node(kId, kUrl);
  tracker.Add(kSyncId, &node, kServerVersion, kCreationTime, specifics);
  const SyncedBookmarkTracker::Entity* entity =
      tracker.GetEntityForSyncId(kSyncId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
  EXPECT_THAT(entity->metadata()->server_id(), Eq(kSyncId));
  EXPECT_THAT(entity->metadata()->server_version(), Eq(kServerVersion));
  EXPECT_THAT(entity->metadata()->creation_time(),
              Eq(syncer::TimeToProtoTime(kCreationTime)));
  syncer::EntityData data;
  *data.specifics.mutable_bookmark() = specifics.bookmark();
  EXPECT_TRUE(entity->MatchesData(data));
  EXPECT_THAT(tracker.GetEntityForSyncId("unknown id"), IsNull());
}

TEST(SyncedBookmarkTrackerTest, ShouldReturnNullForDisassociatedNodes) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, GURL());
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime, specifics);
  ASSERT_THAT(tracker.GetEntityForSyncId(kSyncId), NotNull());
  tracker.Remove(kSyncId);
  EXPECT_THAT(tracker.GetEntityForSyncId(kSyncId), IsNull());
}

TEST(SyncedBookmarkTrackerTest,
     ShouldRequireCommitRequestWhenSequenceNumberIsIncremented) {
  SyncedBookmarkTracker tracker(std::vector<NodeMetadataPair>(),
                                std::make_unique<sync_pb::ModelTypeState>());
  const std::string kSyncId = "SYNC_ID";
  const int64_t kId = 1;
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));
  const sync_pb::EntitySpecifics specifics =
      GenerateSpecifics(/*title=*/std::string(), /*url=*/std::string());
  bookmarks::BookmarkNode node(kId, GURL());
  tracker.Add(kSyncId, &node, kServerVersion, kModificationTime, specifics);

  EXPECT_THAT(tracker.HasLocalChanges(), Eq(false));
  tracker.IncrementSequenceNumber(kSyncId);
  EXPECT_THAT(tracker.HasLocalChanges(), Eq(true));
  // TODO(crbug.com/516866): Test HasLocalChanges after submitting commit
  // request in a separate test probably.
}

}  // namespace

}  // namespace sync_bookmarks
