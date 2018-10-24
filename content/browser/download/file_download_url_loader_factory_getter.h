// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_FILE_DOWNLOAD_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_BROWSER_DOWNLOAD_FILE_DOWNLOAD_URL_LOADER_FACTORY_GETTER_H_

#include "base/files/file_path.h"
#include "components/download/public/common/download_url_loader_factory_getter.h"
#include "url/gurl.h"

namespace content {

// Class for retrieving the URLLoaderFactory for a file URL.
class FileDownloadURLLoaderFactoryGetter
    : public download::DownloadURLLoaderFactoryGetter {
 public:
  FileDownloadURLLoaderFactoryGetter(const GURL& url,
                                     const base::FilePath& profile_path);

  // download::DownloadURLLoaderFactoryGetter implementation.
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;

 protected:
  ~FileDownloadURLLoaderFactoryGetter() override;

 private:
  GURL url_;
  base::FilePath profile_path_;

  DISALLOW_COPY_AND_ASSIGN(FileDownloadURLLoaderFactoryGetter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_FILE_DOWNLOAD_URL_LOADER_FACTORY_GETTER_H_
