// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/url_download_handler.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace network {
struct ResourceResponse;
}

namespace download {

class DownloadDBCache;
class DownloadURLLoaderFactoryGetter;
class DownloadUrlParameters;
class InProgressCache;
struct DownloadDBEntry;

// Manager for handling all active downloads.
class COMPONENTS_DOWNLOAD_EXPORT InProgressDownloadManager
    : public UrlDownloadHandler::Delegate,
      public DownloadItemImplDelegate {
 public:
  using StartDownloadItemCallback =
      base::OnceCallback<void(std::unique_ptr<DownloadCreateInfo> info,
                              DownloadItemImpl*)>;

  // Class to be notified when download starts/stops.
  class COMPONENTS_DOWNLOAD_EXPORT Delegate {
   public:
    // Intercepts the download to another system if applicable. Returns true if
    // the download was intercepted.
    virtual bool InterceptDownload(
        const DownloadCreateInfo& download_create_info) = 0;

    // Gets the default download directory.
    virtual base::FilePath GetDefaultDownloadDirectory() = 0;

    // Gets the download item for the given |download_create_info|.
    // TODO(qinmin): remove this method and let InProgressDownloadManager
    // create the DownloadItem from in-progress cache.
    virtual void StartDownloadItem(
        std::unique_ptr<DownloadCreateInfo> info,
        const DownloadUrlParameters::OnStartedCallback& on_started,
        StartDownloadItemCallback callback) = 0;

    // Gets the URLRequestContextGetter for sending requests.
    // TODO(qinmin): remove this once network service is fully enabled.
    virtual net::URLRequestContextGetter* GetURLRequestContextGetter(
        const DownloadCreateInfo& download_create_info) = 0;

    // Called when all in-progress downloads are loaded from the database.
    virtual void OnInProgressDownloadsLoaded(
        std::vector<std::unique_ptr<download::DownloadItemImpl>>
            in_progress_downloads) = 0;
  };

  using IsOriginSecureCallback = base::RepeatingCallback<bool(const GURL&)>;
  InProgressDownloadManager(Delegate* delegate,
                            const IsOriginSecureCallback& is_origin_secure_cb);
  ~InProgressDownloadManager() override;
  // Called to start a download.
  void BeginDownload(
      std::unique_ptr<DownloadUrlParameters> params,
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
      bool is_new_download,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url);

  // Intercepts a download from navigation.
  void InterceptDownloadFromNavigation(
      std::unique_ptr<network::ResourceRequest> resource_request,
      int render_process_id,
      int render_frame_id,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      std::vector<GURL> url_chain,
      scoped_refptr<network::ResourceResponse> response,
      net::CertStatus cert_status,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter);

  void Initialize(const base::FilePath& metadata_cache_dir,
                  base::OnceClosure callback);

  void StartDownload(
      std::unique_ptr<DownloadCreateInfo> info,
      std::unique_ptr<InputStream> stream,
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
      const DownloadUrlParameters::OnStartedCallback& on_started);

  // Shutting down the manager and stop all downloads.
  void ShutDown();

  // DownloadItemImplDelegate implementations.
  void DetermineDownloadTarget(DownloadItemImpl* download,
                               const DownloadTargetCallback& callback) override;
  void ResumeInterruptedDownload(std::unique_ptr<DownloadUrlParameters> params,
                                 const GURL& site_url) override;
  bool ShouldOpenDownload(DownloadItemImpl* item,
                          const ShouldOpenDownloadCallback& callback) override;
  base::Optional<DownloadEntry> GetInProgressEntry(
      DownloadItemImpl* download) override;
  void ReportBytesWasted(DownloadItemImpl* download) override;

  // Called to remove an in-progress download.
  void RemoveInProgressDownload(const std::string& guid);

  // Called to retrieve an in-progress download.
  DownloadItemImpl* GetInProgressDownload(const std::string& guid);

  void set_file_factory(std::unique_ptr<DownloadFileFactory> file_factory) {
    file_factory_ = std::move(file_factory);
  }
  DownloadFileFactory* file_factory() { return file_factory_.get(); }

  void set_url_loader_factory_getter(
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter) {
    url_loader_factory_getter_ = std::move(url_loader_factory_getter);
  }

 private:
  // UrlDownloadHandler::Delegate implementations.
  void OnUrlDownloadStarted(
      std::unique_ptr<DownloadCreateInfo> download_create_info,
      std::unique_ptr<InputStream> input_stream,
      scoped_refptr<DownloadURLLoaderFactoryGetter> shared_url_loader_factory,
      const DownloadUrlParameters::OnStartedCallback& callback) override;
  void OnUrlDownloadStopped(UrlDownloadHandler* downloader) override;
  void OnUrlDownloadHandlerCreated(
      UrlDownloadHandler::UniqueUrlDownloadHandlerPtr downloader) override;

  // Called download db is initialized.
  void OnDownloadDBInitialized(
      base::OnceClosure callback,
      std::unique_ptr<std::vector<DownloadDBEntry>> entries);

  // Start a DownloadItemImpl.
  void StartDownloadWithItem(
      std::unique_ptr<InputStream> stream,
      scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter,
      std::unique_ptr<DownloadCreateInfo> info,
      DownloadItemImpl* download);

  // Active download handlers.
  std::vector<UrlDownloadHandler::UniqueUrlDownloadHandlerPtr>
      url_download_handlers_;

  // Delegate to provide information to create a new download. Can be null.
  Delegate* delegate_;

  // Factory for the creation of download files.
  std::unique_ptr<DownloadFileFactory> file_factory_;

  // Cache for storing metadata about in progress downloads.
  std::unique_ptr<InProgressCache> download_metadata_cache_;

  // Cache for DownloadDB.
  std::unique_ptr<DownloadDBCache> download_db_cache_;

  // listens to information about in-progress download items.
  std::unique_ptr<DownloadItem::Observer> in_progress_download_observer_;

  // callback to check if an origin is secure.
  IsOriginSecureCallback is_origin_secure_cb_;

  // A list of in-progress download items, could be null if DownloadManagerImpl
  // is managing all downloads.
  std::vector<std::unique_ptr<DownloadItemImpl>> in_progress_downloads_;

  // URLLoaderFactoryGetter for issuing network request when DownloadMangerImpl
  // is not available.
  scoped_refptr<DownloadURLLoaderFactoryGetter> url_loader_factory_getter_;

  base::WeakPtrFactory<InProgressDownloadManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InProgressDownloadManager);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_IN_PROGRESS_DOWNLOAD_MANAGER_H_
