// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/file_download_url_loader_factory_getter.h"

#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/url_loader_factory_getter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

FileDownloadURLLoaderFactoryGetter::FileDownloadURLLoaderFactoryGetter(
    const GURL& url,
    const base::FilePath& profile_path)
    : url_(url), profile_path_(profile_path) {
  DCHECK(url.SchemeIs(url::kFileScheme));
}

FileDownloadURLLoaderFactoryGetter::~FileDownloadURLLoaderFactoryGetter() =
    default;

scoped_refptr<network::SharedURLLoaderFactory>
FileDownloadURLLoaderFactoryGetter::GetURLLoaderFactory() {
  DCHECK(download::GetIOTaskRunner()->BelongsToCurrentThread());

  network::mojom::URLLoaderFactoryPtrInfo url_loader_factory_ptr_info;
  mojo::MakeStrongBinding(
      std::make_unique<FileURLLoaderFactory>(
          profile_path_, base::CreateSequencedTaskRunnerWithTraits(
                             {base::MayBlock(), base::TaskPriority::BACKGROUND,
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      MakeRequest(&url_loader_factory_ptr_info));

  return base::MakeRefCounted<network::WrapperSharedURLLoaderFactory>(
      std::move(url_loader_factory_ptr_info));
}

}  // namespace content
