// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/provider/bookmark_model_task.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/model_loader.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using content::BrowserThread;

BookmarkModelTask::BookmarkModelTask(BookmarkModel* model) : model_(model) {
  // Ensure the initialization of the native bookmark model.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(model_);
  model_->model_loader()->BlockTillLoaded();
}

BookmarkModel* BookmarkModelTask::model() const {
  return model_;
}
