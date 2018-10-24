// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/synced_bookmark_tracker.h"

#include <utility>

#include "base/base64.h"
#include "base/sha1.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/time.h"
#include "components/sync/model/entity_data.h"

namespace sync_bookmarks {

namespace {

void HashSpecifics(const sync_pb::EntitySpecifics& specifics,
                   std::string* hash) {
  DCHECK_GT(specifics.ByteSize(), 0);
  base::Base64Encode(base::SHA1HashString(specifics.SerializeAsString()), hash);
}

}  // namespace

SyncedBookmarkTracker::Entity::Entity(
    const bookmarks::BookmarkNode* bookmark_node,
    std::unique_ptr<sync_pb::EntityMetadata> metadata)
    : bookmark_node_(bookmark_node), metadata_(std::move(metadata)) {
  if (bookmark_node) {
    DCHECK(!metadata_->is_deleted());
  } else {
    DCHECK(metadata_->is_deleted());
  }
}

SyncedBookmarkTracker::Entity::~Entity() = default;

bool SyncedBookmarkTracker::Entity::IsUnsynced() const {
  return metadata_->sequence_number() > metadata_->acked_sequence_number();
}

bool SyncedBookmarkTracker::Entity::MatchesData(
    const syncer::EntityData& data) const {
  if (metadata_->is_deleted() || data.is_deleted()) {
    // In case of deletion, no need to check the specifics.
    return metadata_->is_deleted() == data.is_deleted();
  }
  return MatchesSpecificsHash(data.specifics);
}

bool SyncedBookmarkTracker::Entity::MatchesSpecificsHash(
    const sync_pb::EntitySpecifics& specifics) const {
  DCHECK(!metadata_->is_deleted());
  DCHECK_GT(specifics.ByteSize(), 0);
  std::string hash;
  HashSpecifics(specifics, &hash);
  return hash == metadata_->specifics_hash();
}

SyncedBookmarkTracker::SyncedBookmarkTracker(
    std::vector<NodeMetadataPair> nodes_metadata,
    std::unique_ptr<sync_pb::ModelTypeState> model_type_state)
    : model_type_state_(std::move(model_type_state)) {
  for (NodeMetadataPair& node_metadata : nodes_metadata) {
    const std::string& sync_id = node_metadata.second->server_id();
    sync_id_to_entities_map_[sync_id] = std::make_unique<Entity>(
        node_metadata.first, std::move(node_metadata.second));
  }
}

SyncedBookmarkTracker::~SyncedBookmarkTracker() = default;

const SyncedBookmarkTracker::Entity* SyncedBookmarkTracker::GetEntityForSyncId(
    const std::string& sync_id) const {
  auto it = sync_id_to_entities_map_.find(sync_id);
  return it != sync_id_to_entities_map_.end() ? it->second.get() : nullptr;
}

void SyncedBookmarkTracker::Add(const std::string& sync_id,
                                const bookmarks::BookmarkNode* bookmark_node,
                                int64_t server_version,
                                base::Time creation_time,
                                const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  auto metadata = std::make_unique<sync_pb::EntityMetadata>();
  metadata->set_is_deleted(false);
  metadata->set_server_id(sync_id);
  metadata->set_server_version(server_version);
  metadata->set_creation_time(syncer::TimeToProtoTime(creation_time));
  metadata->set_sequence_number(0);
  metadata->set_acked_sequence_number(0);
  HashSpecifics(specifics, metadata->mutable_specifics_hash());
  sync_id_to_entities_map_[sync_id] =
      std::make_unique<Entity>(bookmark_node, std::move(metadata));
}

void SyncedBookmarkTracker::Update(const std::string& sync_id,
                                   int64_t server_version,
                                   base::Time modification_time,
                                   const sync_pb::EntitySpecifics& specifics) {
  DCHECK_GT(specifics.ByteSize(), 0);
  auto it = sync_id_to_entities_map_.find(sync_id);
  Entity* entity = it->second.get();
  DCHECK(entity);
  entity->metadata()->set_server_id(sync_id);
  entity->metadata()->set_server_version(server_version);
  entity->metadata()->set_modification_time(
      syncer::TimeToProtoTime(modification_time));
  HashSpecifics(specifics, entity->metadata()->mutable_specifics_hash());
}

void SyncedBookmarkTracker::Remove(const std::string& sync_id) {
  sync_id_to_entities_map_.erase(sync_id);
}

void SyncedBookmarkTracker::IncrementSequenceNumber(
    const std::string& sync_id) {
  Entity* entity = sync_id_to_entities_map_.find(sync_id)->second.get();
  DCHECK(entity);
  DCHECK(!entity->metadata()->is_deleted());
  // TODO(crbug.com/516866): Update base hash specifics here if the entity is
  // not already out of sync.
  entity->metadata()->set_sequence_number(
      entity->metadata()->sequence_number() + 1);
}

sync_pb::BookmarkModelMetadata
SyncedBookmarkTracker::BuildBookmarkModelMetadata() const {
  sync_pb::BookmarkModelMetadata model_metadata;
  for (const std::pair<const std::string, std::unique_ptr<Entity>>& pair :
       sync_id_to_entities_map_) {
    sync_pb::BookmarkMetadata* bookmark_metadata =
        model_metadata.add_bookmarks_metadata();
    if (pair.second->bookmark_node() != nullptr) {
      bookmark_metadata->set_id(pair.second->bookmark_node()->id());
    }
    *bookmark_metadata->mutable_metadata() = *pair.second->metadata();
  }
  *model_metadata.mutable_model_type_state() = *model_type_state_;
  return model_metadata;
}

bool SyncedBookmarkTracker::HasLocalChanges() const {
  for (const std::pair<const std::string, std::unique_ptr<Entity>>& pair :
       sync_id_to_entities_map_) {
    Entity* entity = pair.second.get();
    if (entity->IsUnsynced()) {
      return true;
    }
  }
  return false;
}

std::size_t SyncedBookmarkTracker::TrackedEntitiesCountForTest() const {
  return sync_id_to_entities_map_.size();
}

}  // namespace sync_bookmarks
