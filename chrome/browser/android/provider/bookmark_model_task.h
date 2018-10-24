// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_TASK_H_
#define CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_TASK_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"

// Base class for synchronous tasks that involve the bookmark model.
// Ensures the model has been loaded before accessing it.
// Must not be created from the UI thread.
class BookmarkModelTask {
 public:
  explicit BookmarkModelTask(bookmarks::BookmarkModel* model);
  bookmarks::BookmarkModel* model() const;

 private:
  bookmarks::BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTask);
};

#endif  // CHROME_BROWSER_ANDROID_PROVIDER_BOOKMARK_MODEL_TASK_H_
