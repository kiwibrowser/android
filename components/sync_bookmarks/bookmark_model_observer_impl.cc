// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_observer_impl.h"

namespace sync_bookmarks {

BookmarkModelObserverImpl::BookmarkModelObserverImpl(
    const base::RepeatingClosure& nudge_for_commit_closure,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_tracker_(bookmark_tracker),
      nudge_for_commit_closure_(nudge_for_commit_closure) {
  DCHECK(bookmark_tracker_);
}

BookmarkModelObserverImpl::~BookmarkModelObserverImpl() = default;

void BookmarkModelObserverImpl::BookmarkModelLoaded(
    bookmarks::BookmarkModel* model,
    bool ids_reassigned) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkModelBeingDeleted(
    bookmarks::BookmarkModel* model) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    int old_index,
    const bookmarks::BookmarkNode* new_parent,
    int new_index) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int index) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    int old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkAllUserNodesRemoved(
    bookmarks::BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkNodeFaviconChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  NOTIMPLEMENTED();
}

void BookmarkModelObserverImpl::BookmarkNodeChildrenReordered(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  NOTIMPLEMENTED();
}

}  // namespace sync_bookmarks
