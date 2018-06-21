// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_

#include <set>

#include "base/callback.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "url/gurl.h"

namespace sync_bookmarks {

class SyncedBookmarkTracker;

// Class for listening to local changes in the bookmark model and updating
// metadata in SyncedBookmarkTracker, such that ultimately the processor exposes
// those local changes to the sync engine.
class BookmarkModelObserverImpl : public bookmarks::BookmarkModelObserver {
 public:
  // |bookmark_tracker_| must not be null and must outlive this object.
  BookmarkModelObserverImpl(
      const base::RepeatingClosure& nudge_for_commit_closure,
      SyncedBookmarkTracker* bookmark_tracker);
  ~BookmarkModelObserverImpl() override;

  // BookmarkModelObserver:
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* old_parent,
                         int old_index,
                         const bookmarks::BookmarkNode* new_parent,
                         int new_index) override;
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         int index) override;
  void BookmarkNodeRemoved(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* parent,
                           int old_index,
                           const bookmarks::BookmarkNode* node,
                           const std::set<GURL>& removed_urls) override;
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                   const std::set<GURL>& removed_urls) override;
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
                           const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override;

 private:
  // Points to the tracker owned by the processor. It keeps the mapping between
  // bookmark nodes and corresponding sync server entities.
  SyncedBookmarkTracker* const bookmark_tracker_;

  // The callback used to inform the sync engine that there are local changes to
  // be committed.
  const base::RepeatingClosure nudge_for_commit_closure_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelObserverImpl);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_OBSERVER_IMPL_H_
