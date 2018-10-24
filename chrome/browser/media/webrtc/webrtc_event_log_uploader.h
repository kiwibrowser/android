// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

// A class implementing this interace can register for notification of an
// upload's eventual result (success/failure).
class WebRtcEventLogUploaderObserver {
 public:
  virtual void OnWebRtcEventLogUploadComplete(const base::FilePath& log_file,
                                              bool upload_successful) = 0;

 protected:
  virtual ~WebRtcEventLogUploaderObserver() = default;
};

// A sublcass of this interface would take ownership of a file, and either
// upload it to a remote server (actual implementation), or pretend to do
// so (in unit tests). It will typically take on an observer of type
// WebRtcEventLogUploaderObserver, and inform it of the success or failure
// of the upload.
class WebRtcEventLogUploader {
 public:
  // Since we'll need more than one instance of the abstract
  // WebRtcEventLogUploader, we'll need an abstract factory for it.
  class Factory {
   public:
    virtual ~Factory() = default;

    // Creates uploaders. The observer is passed to each call of Create,
    // rather than be memorized by the factory's constructor, because factories
    // created by unit tests have no visibility into the real implementation's
    // observer (WebRtcRemoteEventLogManager).
    // This takes ownership of the file. The caller must not attempt to access
    // the file after invoking Create().
    virtual std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        WebRtcEventLogUploaderObserver* observer) = 0;
  };

  virtual ~WebRtcEventLogUploader() = default;

  // Getter for the details of the file this uploader is handling.
  // Can be called for ongoing, completed, failed or cancelled uploads.
  virtual const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const = 0;

  // Cancels the upload. Returns true if the upload was cancelled due to this
  // call, and false if the upload was already completed or aborted before this
  // call. (Aborted uploads are ones where the file could not be read, etc.)
  virtual bool Cancel() = 0;
};

// Primary implementation of WebRtcEventLogUploader. Uploads log files to crash.
// Deletes log files whether they were successfully uploaded or not.
class WebRtcEventLogUploaderImpl : public WebRtcEventLogUploader {
 public:
  class Factory : public WebRtcEventLogUploader::Factory {
   public:
    explicit Factory(net::URLRequestContextGetter* request_context_getter);

    ~Factory() override = default;

    std::unique_ptr<WebRtcEventLogUploader> Create(
        const WebRtcLogFileInfo& log_file,
        WebRtcEventLogUploaderObserver* observer) override;

   protected:
    friend class WebRtcEventLogUploaderImplTest;

    std::unique_ptr<WebRtcEventLogUploader> CreateWithCustomMaxSizeForTesting(
        const WebRtcLogFileInfo& log_file,
        WebRtcEventLogUploaderObserver* observer,
        size_t max_remote_log_file_size_bytes);

   private:
    net::URLRequestContextGetter* const request_context_getter_;
  };

  WebRtcEventLogUploaderImpl(
      net::URLRequestContextGetter* request_context_getter,
      const WebRtcLogFileInfo& log_file,
      WebRtcEventLogUploaderObserver* observer,
      size_t max_remote_log_file_size_bytes);
  ~WebRtcEventLogUploaderImpl() override;

  const WebRtcLogFileInfo& GetWebRtcLogFileInfo() const override;

  bool Cancel() override;

 protected:
  friend class WebRtcEventLogUploaderImplTest;

  // Primes the log file for uploading. Returns true if the file could be read,
  // in which case |upload_data| will be populated with the data to be uploaded
  // (both the log file's contents as well as metadata for Crash).
  // TODO(crbug.com/775415): Avoid reading the entire file into memory.
  bool PrepareUploadData(std::string* upload_data);

  // Initiates the file's upload.
  void StartUpload(const std::string& upload_data);

  // Before this is called, other methods of the URLFetcherDelegate API may be
  // called, but this is guaranteed to be the last call, so deleting |this| is
  // permissible afterwards.
  void OnURLFetchComplete(const net::URLFetcher* source);

  // Cleanup and reporting to |observer_|.
  void ReportResult(bool result);

  // Remove the log file which is owned by |this|.
  void DeleteLogFile();

  // Allows testing the behavior for excessively large files.
  void SetMaxRemoteLogFileSizeBytesForTesting(size_t max_size_bytes);

  // The URL used for uploading the logs.
  static const char kUploadURL[];

 private:
  class Delegate : public net::URLFetcherDelegate {
   public:
    explicit Delegate(WebRtcEventLogUploaderImpl* owner);
    ~Delegate() override = default;

    // net::URLFetcherDelegate implementation.
#if DCHECK_IS_ON()
    void OnURLFetchUploadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total) override;
#endif
    void OnURLFetchComplete(const net::URLFetcher* source) override;

   private:
    WebRtcEventLogUploaderImpl* const owner_;
  } delegate_;

  // Supplier of URLRequestContext objects, which are used by |url_fetcher_|.
  // They must outlive |this|.
  net::URLRequestContextGetter* const request_context_getter_;

  // Housekeeping information about the uploaded file (path, time of last
  // modification, associated BrowserContext).
  const WebRtcLogFileInfo log_file_;

  // The observer to be notified when this upload succeeds or fails.
  WebRtcEventLogUploaderObserver* const observer_;

  // Maximum allowed file size. In production code, this is a hard-coded,
  // but unit tests may set other values.
  const size_t max_log_file_size_bytes_;

  // This object is in charge of the actual upload.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // The object lives on this IO-capable task runner.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_WEBRTC_EVENT_LOG_UPLOADER_H_
