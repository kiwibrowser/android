// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_sync_service.h"

#include "base/feature_list.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync_bookmarks/bookmark_model_type_processor.h"
#include "components/undo/bookmark_undo_service.h"

namespace sync_bookmarks {

BookmarkSyncService::BookmarkSyncService(
    BookmarkUndoService* bookmark_undo_service) {
  if (base::FeatureList::IsEnabled(switches::kSyncUSSBookmarks)) {
    bookmark_model_type_processor_ =
        std::make_unique<sync_bookmarks::BookmarkModelTypeProcessor>(
            bookmark_undo_service);
  }
}

BookmarkSyncService::~BookmarkSyncService() {}

void BookmarkSyncService::Shutdown() {}

std::string BookmarkSyncService::EncodeBookmarkSyncMetadata() {
  if (!bookmark_model_type_processor_) {
    return std::string();
  }
  return bookmark_model_type_processor_->EncodeSyncMetadata();
}

void BookmarkSyncService::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure,
    bookmarks::BookmarkModel* model) {
  if (bookmark_model_type_processor_) {
    bookmark_model_type_processor_->DecodeSyncMetadata(
        metadata_str, schedule_save_closure, model);
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
BookmarkSyncService::GetBookmarkSyncControllerDelegateOnUIThread() {
  if (!bookmark_model_type_processor_) {
    return nullptr;
  }
  return bookmark_model_type_processor_->GetWeakPtr();
}

}  // namespace sync_bookmarks
