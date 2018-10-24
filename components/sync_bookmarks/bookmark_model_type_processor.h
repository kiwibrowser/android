// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

class BookmarkUndoService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

namespace sync_bookmarks {

class BookmarkModelObserverImpl;

class BookmarkModelTypeProcessor : public syncer::ModelTypeProcessor,
                                   public syncer::ModelTypeControllerDelegate {
 public:
  // |bookmark_undo_service| must not be nullptr and must outlive this object.
  explicit BookmarkModelTypeProcessor(
      BookmarkUndoService* bookmark_undo_service);
  ~BookmarkModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<syncer::CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       const GetLocalChangesCallback& callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const syncer::CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        const syncer::UpdateResponseDataList& updates) override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const syncer::DataTypeActivationRequest& request,
                      StartCallback start_callback) override;
  void OnSyncStopping(syncer::SyncStopMetadataFate metadata_fate) override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetStatusCountersForDebugging(StatusCountersCallback callback) override;
  void RecordMemoryUsageHistogram() override;

  // Encodes all sync metadata into a string, representing a state that can be
  // restored via DecodeSyncMetadata() below.
  std::string EncodeSyncMetadata() const;

  // It mainly decodes a BookmarkModelMetadata proto seralized in
  // |metadata_str|, and uses it to fill in the tracker and the model type state
  // objects. |model| must not be null and must outlive this object. It is used
  // to the retrieve the local node ids, and is stored in the processor to be
  // used for further model operations. |schedule_save_closure| is a repeating
  // closure used to schedule a save of the bookmark model together with the
  // metadata.
  void DecodeSyncMetadata(const std::string& metadata_str,
                          const base::RepeatingClosure& schedule_save_closure,
                          bookmarks::BookmarkModel* model);

  // Public for testing.
  static std::vector<const syncer::UpdateResponseData*> ReorderUpdatesForTest(
      const syncer::UpdateResponseDataList& updates);

  const SyncedBookmarkTracker* GetTrackerForTest() const;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Reorders incoming updates such that parent creation is before child
  // creation and child deletion is before parent deletion, and deletions should
  // come last. The returned pointers point to the elements in the original
  // |updates|.
  static std::vector<const syncer::UpdateResponseData*> ReorderUpdates(
      const syncer::UpdateResponseDataList& updates);

  // Given a remote update entity, it returns the parent bookmark node of the
  // corresponding node. It returns null if the parent node cannot be found.
  const bookmarks::BookmarkNode* GetParentNode(
      const syncer::EntityData& update_entity) const;

  // Processes a remote creation of a bookmark node.
  // 1. For permanent folders, they are only registered in |bookmark_tracker_|.
  // 2. If the nodes parent cannot be found, the remote creation update is
  //    ignored.
  // 3. Otherwise, a new node is created in the local bookmark model and
  //    registered in |bookmark_tracker_|.
  void ProcessRemoteCreate(const syncer::UpdateResponseData& update);

  // Processes a remote update of a bookmark node. |update| must not be a
  // deletion, and the server_id must be already tracked, otherwise, it is a
  // creation that gets handeled in ProcessRemoteCreate(). |tracked_entity| is
  // the tracked entity for that server_id. It is passed as a dependency instead
  // of performing a lookup inside ProcessRemoteUpdate() to avoid wasting CPU
  // cycles for doing another lookup (this code runs on the UI thread).
  void ProcessRemoteUpdate(const syncer::UpdateResponseData& update,
                           const SyncedBookmarkTracker::Entity* tracked_entity);

  // Process a remote delete of a bookmark node. |update_entity| must not be a
  // deletion. |tracked_entity| is the tracked entity for that server_id. It is
  // passed as a dependency instead of performing a lookup inside
  // ProcessRemoteDelete() to avoid wasting CPU cycles for doing another lookup
  // (this code runs on the UI thread).
  void ProcessRemoteDelete(const syncer::EntityData& update_entity,
                           const SyncedBookmarkTracker::Entity* tracked_entity);

  // Associates the permanent bookmark folders with the corresponding server
  // side ids and registers the association in |bookmark_tracker_|.
  // |update_entity| must contain server_defined_unique_tag that is used to
  // determine the corresponding permanent node. All permanent nodes are assumed
  // to be directly children nodes of |kBookmarksRootId|. This method is used in
  // the initial sync cycle only.
  void AssociatePermanentFolder(const syncer::UpdateResponseData& update);

  // If preconditions are met, inform sync that we are ready to connect.
  void ConnectIfReady();

  // Nudges worker if there are any local entities to be committed. Should only
  // be called after initial sync is done and processor is tracking sync
  // entities.
  void NudgeForCommitIfNeeded();

  // Stores the start callback in between OnSyncStarting() and
  // DecodeSyncMetadata().
  StartCallback start_callback_;

  // The bookmark model we are processing local changes from and forwarding
  // remote changes to. It is set during DecodeSyncMetadata(), which is called
  // during startup, as part of the bookmark-loading process.
  bookmarks::BookmarkModel* bookmark_model_ = nullptr;

  // Used to suspend bookmark undo when processing remote changes.
  BookmarkUndoService* const bookmark_undo_service_;

  // The callback used to schedule the persistence of bookmark model as well as
  // the metadata to a file during which latest metadata should also be pulled
  // via EncodeSyncMetadata. Processor should invoke it upon changes in the
  // metadata that don't imply changes in the model itself. Persisting updates
  // that imply model changes is the model's responsibility.
  base::RepeatingClosure schedule_save_closure_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<syncer::CommitQueue> worker_;

  // Keeps the mapping between server ids and bookmarks nodes together with sync
  // metadata. It is constructed and set during DecodeSyncMetadata(), which is
  // called during startup, as part of the bookmark-loading process.
  std::unique_ptr<SyncedBookmarkTracker> bookmark_tracker_;

  std::unique_ptr<BookmarkModelObserverImpl> bookmark_model_observer_;

  base::WeakPtrFactory<BookmarkModelTypeProcessor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTypeProcessor);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
