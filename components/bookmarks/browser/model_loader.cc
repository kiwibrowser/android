// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/model_loader.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/browser/url_index.h"

namespace bookmarks {

ModelLoader::ModelLoader(const base::FilePath& profile_path,
                         base::SequencedTaskRunner* load_sequenced_task_runner,
                         std::unique_ptr<BookmarkLoadDetails> details,
                         LoadCallback callback)
    : loaded_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                     base::WaitableEvent::InitialState::NOT_SIGNALED) {
  load_sequenced_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ModelLoader::DoLoadOnBackgroundThread, this, profile_path,
                     base::ThreadTaskRunnerHandle::Get(), std::move(details),
                     std::move(callback)));
}

void ModelLoader::BlockTillLoaded() {
  loaded_signal_.Wait();
}

ModelLoader::~ModelLoader() = default;

void ModelLoader::DoLoadOnBackgroundThread(
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> main_sequenced_task_runner,
    std::unique_ptr<BookmarkLoadDetails> details,
    LoadCallback callback) {
  LoadBookmarks(profile_path, details.get());
  history_bookmark_model_ = details->url_index();
  loaded_signal_.Signal();
  main_sequenced_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ModelLoader::OnFinishedLoad, this,
                                std::move(details), std::move(callback)));
}

void ModelLoader::OnFinishedLoad(std::unique_ptr<BookmarkLoadDetails> details,
                                 LoadCallback callback) {
  std::move(callback).Run(std::move(details));
}

}  // namespace bookmarks
