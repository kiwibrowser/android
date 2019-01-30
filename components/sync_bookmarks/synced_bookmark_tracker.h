// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
#define COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/sync/protocol/bookmark_model_metadata.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"

namespace bookmarks {
class BookmarkNode;
}

namespace syncer {
struct EntityData;
}

namespace sync_bookmarks {

using NodeMetadataPair = std::pair<const bookmarks::BookmarkNode*,
                                   std::unique_ptr<sync_pb::EntityMetadata>>;

// This class is responsible for keeping the mapping between bookmarks node in
// the local model and the server-side corresponding sync entities. It manages
// the metadata for its entity and caches entity data upon a local change until
// commit confirmation is received.
class SyncedBookmarkTracker {
 public:
  class Entity {
   public:
    // |bookmark_node| can be null for tombstones. |metadata| must not be null.
    Entity(const bookmarks::BookmarkNode* bookmark_node,
           std::unique_ptr<sync_pb::EntityMetadata> metadata);
    ~Entity();

    // Returns true if this data is out of sync with the server.
    // A commit may or may not be in progress at this time.
    bool IsUnsynced() const;

    // Check whether |data| matches the stored specifics hash.
    bool MatchesData(const syncer::EntityData& data) const;

    // Returns null for tomstones.
    const bookmarks::BookmarkNode* bookmark_node() const {
      return bookmark_node_;
    }

    const sync_pb::EntityMetadata* metadata() const { return metadata_.get(); }
    sync_pb::EntityMetadata* metadata() { return metadata_.get(); }

   private:
    // Check whether |specifics| matches the stored specifics_hash.
    bool MatchesSpecificsHash(const sync_pb::EntitySpecifics& specifics) const;

    const bookmarks::BookmarkNode* const bookmark_node_;

    // Serializable Sync metadata.
    std::unique_ptr<sync_pb::EntityMetadata> metadata_;

    DISALLOW_COPY_AND_ASSIGN(Entity);
  };

  SyncedBookmarkTracker(
      std::vector<NodeMetadataPair> nodes_metadata,
      std::unique_ptr<sync_pb::ModelTypeState> model_type_state);
  ~SyncedBookmarkTracker();

  // Returns null if not entity is found.
  const Entity* GetEntityForSyncId(const std::string& sync_id) const;

  // Adds an entry for the |sync_id| and the corresponding local bookmark node
  // and metadata in |sync_id_to_entities_map_|.
  void Add(const std::string& sync_id,
           const bookmarks::BookmarkNode* bookmark_node,
           int64_t server_version,
           base::Time modification_time,
           const sync_pb::EntitySpecifics& specifics);

  // Adds an existing entry for the |sync_id| and the corresponding metadata in
  // |sync_id_to_entities_map_|.
  void Update(const std::string& sync_id,
              int64_t server_version,
              base::Time modification_time,
              const sync_pb::EntitySpecifics& specifics);

  // Removes the entry coressponding to the |sync_id| from
  // |sync_id_to_entities_map_|.
  void Remove(const std::string& sync_id);

  // Increment sequence number in the metadata for the entity with |sync_id|.
  // Tracker must contain a non-tomstone entity with server id = |sync_id|.
  void IncrementSequenceNumber(const std::string& sync_id);

  sync_pb::BookmarkModelMetadata BuildBookmarkModelMetadata() const;

  // Returns true if there are any local entities to be committed.
  bool HasLocalChanges() const;

  // Returns number of tracked entities. Used only in test.
  std::size_t TrackedEntitiesCountForTest() const;

 private:
  // A map of sync server ids to sync entities. This should contain entries and
  // metadata for almost everything. However, since local data are loaded only
  // when needed (e.g. before a commit cycle), the entities may not always
  // contain model type data/specifics.
  std::map<std::string, std::unique_ptr<Entity>> sync_id_to_entities_map_;

  // The model metadata (progress marker, initial sync done, etc).
  std::unique_ptr<sync_pb::ModelTypeState> model_type_state_;

  DISALLOW_COPY_AND_ASSIGN(SyncedBookmarkTracker);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_SYNCED_BOOKMARK_TRACKER_H_
