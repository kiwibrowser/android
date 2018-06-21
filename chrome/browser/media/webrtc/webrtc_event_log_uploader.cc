// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"

namespace {
// TODO(crbug.com/817495): Eliminate the duplication with other uploaders.
const char kUploadContentType[] = "multipart/form-data";
const char kBoundary[] = "----**--yradnuoBgoLtrapitluMklaTelgooG--**----";

const char kLogFilename[] = "webrtc_event_log";
const char kLogExtension[] = "log";

constexpr size_t kExpectedMimeOverheadBytes = 1000;  // Intentional overshot.

// TODO(crbug.com/817495): Eliminate the duplication with other uploaders.
#if defined(OS_WIN)
const char kProduct[] = "Chrome";
#elif defined(OS_MACOSX)
const char kProduct[] = "Chrome_Mac";
#elif defined(OS_LINUX)
const char kProduct[] = "Chrome_Linux";
#elif defined(OS_ANDROID)
const char kProduct[] = "Chrome_Android";
#elif defined(OS_CHROMEOS)
const char kProduct[] = "Chrome_ChromeOS";
#else
#error Platform not supported.
#endif

// TODO(crbug.com/775415): Update comment to reflect new policy when discarding
// the command line flag.
constexpr net::NetworkTrafficAnnotationTag
    kWebrtcEventLogUploaderTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("webrtc_event_log_uploader", R"(
      semantics {
        sender: "WebRTC Event Log uploader module"
        description:
          "Uploads a WebRTC event log to a server called Crash. These logs "
          "will not contain private information. They will be used to "
          "improve WebRTC (fix bugs, tune performance, etc.)."
        trigger:
          "A privileged JS application (Hangouts/Meet) has requested a peer "
          "connection to be logged, and the resulting event log to be "
          "uploaded at a time deemed to cause the least interference to the "
          "user (i.e., when the user is not busy making other VoIP calls)."
        data:
          "WebRTC events such as the timing of audio playout (but not the "
          "content), timing and size of RTP packets sent/received, etc."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "This feature is only enabled if the user launches Chrome "
                 "with a specific command line flag: "
                 "--enable-features=WebRtcRemoteEventLog"
        policy_exception_justification:
          "Not applicable."
      })");

void AddFileContents(const std::string& file_contents,
                     const std::string& content_type,
                     std::string* post_data) {
  // net::AddMultipartValueForUpload does almost what we want to do here, except
  // that it does not add the "filename" attribute. We hack it to force it to.
  std::string mime_value_name = base::StringPrintf(
      "%s\"; filename=\"%s.%s\"", kLogFilename, kLogFilename, kLogExtension);
  net::AddMultipartValueForUpload(mime_value_name, file_contents, kBoundary,
                                  content_type, post_data);
}

std::string MimeContentType() {
  const char kBoundaryKeywordAndMisc[] = "; boundary=";

  std::string content_type;
  content_type.reserve(sizeof(content_type) + sizeof(kBoundaryKeywordAndMisc) +
                       sizeof(kBoundary));

  content_type.append(kUploadContentType);
  content_type.append(kBoundaryKeywordAndMisc);
  content_type.append(kBoundary);

  return content_type;
}
}  // namespace

const char WebRtcEventLogUploaderImpl::kUploadURL[] =
    "https://clients2.google.com/cr/report";

WebRtcEventLogUploaderImpl::Factory::Factory(
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter) {}

std::unique_ptr<WebRtcEventLogUploader>
WebRtcEventLogUploaderImpl::Factory::Create(
    const WebRtcLogFileInfo& log_file,
    WebRtcEventLogUploaderObserver* observer) {
  DCHECK(observer);
  return std::make_unique<WebRtcEventLogUploaderImpl>(
      request_context_getter_, log_file, observer, kMaxRemoteLogFileSizeBytes);
}

std::unique_ptr<WebRtcEventLogUploader>
WebRtcEventLogUploaderImpl::Factory::CreateWithCustomMaxSizeForTesting(
    const WebRtcLogFileInfo& log_file,
    WebRtcEventLogUploaderObserver* observer,
    size_t max_log_file_size_bytes) {
  DCHECK(observer);
  return std::make_unique<WebRtcEventLogUploaderImpl>(
      request_context_getter_, log_file, observer, max_log_file_size_bytes);
}

WebRtcEventLogUploaderImpl::Delegate::Delegate(
    WebRtcEventLogUploaderImpl* owner)
    : owner_(owner) {}

#if DCHECK_IS_ON()
void WebRtcEventLogUploaderImpl::Delegate::OnURLFetchUploadProgress(
    const net::URLFetcher* source,
    int64_t current,
    int64_t total) {
  std::string unit;
  if (total <= 1000) {
    unit = "bytes";
  } else if (total <= 1000 * 1000) {
    unit = "KBs";
    current /= 1000;
    total /= 1000;
  } else {
    unit = "MBs";
    current /= 1000 * 1000;
    total /= 1000 * 1000;
  }
  VLOG(1) << "WebRTC event log upload progress: " << current << " / " << total
          << " " << unit << ".";
}
#endif

void WebRtcEventLogUploaderImpl::Delegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  owner_->OnURLFetchComplete(source);
}

WebRtcEventLogUploaderImpl::WebRtcEventLogUploaderImpl(
    net::URLRequestContextGetter* request_context_getter,
    const WebRtcLogFileInfo& log_file,
    WebRtcEventLogUploaderObserver* observer,
    size_t max_log_file_size_bytes)
    : delegate_(this),
      request_context_getter_(request_context_getter),
      log_file_(log_file),
      observer_(observer),
      max_log_file_size_bytes_(max_log_file_size_bytes),
      io_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(observer);

  std::string upload_data;

  if (!PrepareUploadData(&upload_data)) {
    ReportResult(false);
    return;
  }

  StartUpload(upload_data);
}

WebRtcEventLogUploaderImpl::~WebRtcEventLogUploaderImpl() {
  // WebRtcEventLogUploaderImpl objects' deletion scenarios:
  // 1. Upload started and finished - |url_fetcher_| should have been reset
  //    so that we would be able to DCHECK and demonstrate that the determinant
  //    is maintained.
  // 2. Upload started and cancelled - behave similarly to a finished upload.
  // 3. The upload was never started, due to an early failure (e.g. file not
  //    found). In that case, |url_fetcher_| will not have been set.
  // 4. Chrome shutdown.
  if (io_task_runner_->RunsTasksInCurrentSequence()) {  // Scenarios 1-3.
    DCHECK(!url_fetcher_);
  } else {  // # Scenario #4 - Chrome shutdown.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    bool will_delete =
        io_task_runner_->DeleteSoon(FROM_HERE, url_fetcher_.release());
    DCHECK(!will_delete)
        << "Task runners must have been stopped by this stage of shutdown.";
  }
}

const WebRtcLogFileInfo& WebRtcEventLogUploaderImpl::GetWebRtcLogFileInfo()
    const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  return log_file_;
}

bool WebRtcEventLogUploaderImpl::Cancel() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (!url_fetcher_) {
    // The upload either already completed, or was never properly started (due
    // to a file read failure, etc.).
    return false;
  }

  // Note that in this case, it might still be that the last bytes hit the
  // wire right as we attempt to cancel the upload. OnURLFetchComplete, however,
  // would not be called.
  url_fetcher_.reset();
  DeleteLogFile();
  return true;
}

bool WebRtcEventLogUploaderImpl::PrepareUploadData(std::string* upload_data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  std::string log_file_contents;
  if (!base::ReadFileToStringWithMaxSize(log_file_.path, &log_file_contents,
                                         max_log_file_size_bytes_)) {
    LOG(WARNING) << "Couldn't read event log file, or max file size exceeded.";
    return false;
  }

  DCHECK(upload_data->empty());
  upload_data->reserve(log_file_contents.size() + kExpectedMimeOverheadBytes);
  net::AddMultipartValueForUpload("prod", kProduct, kBoundary, "", upload_data);
  net::AddMultipartValueForUpload("ver",
                                  version_info::GetVersionNumber() + "-webrtc",
                                  kBoundary, "", upload_data);
  net::AddMultipartValueForUpload("guid", "0", kBoundary, "", upload_data);
  net::AddMultipartValueForUpload("type", kLogFilename, kBoundary, "",
                                  upload_data);
  AddFileContents(log_file_contents, "application/log", upload_data);
  net::AddMultipartFinalDelimiterForUpload(kBoundary, upload_data);

  return true;
}

void WebRtcEventLogUploaderImpl::StartUpload(const std::string& upload_data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  url_fetcher_ = net::URLFetcher::Create(
      GURL(kUploadURL), net::URLFetcher::POST, &delegate_,
      kWebrtcEventLogUploaderTrafficAnnotation);
  url_fetcher_->SetRequestContext(request_context_getter_);
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES);
  url_fetcher_->SetUploadData(MimeContentType(), upload_data);
  url_fetcher_->Start();  // Delegat::OnURLFetchComplete called when finished.
}

void WebRtcEventLogUploaderImpl::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(url_fetcher_);
  DCHECK_EQ(source, url_fetcher_.get());

  const bool upload_successful =
      (source->GetStatus().status() == net::URLRequestStatus::SUCCESS &&
       source->GetResponseCode() == net::HTTP_OK);

  if (upload_successful) {
    // TODO(crbug.com/775415): Update chrome://webrtc-logs.
    std::string report_id;
    if (!url_fetcher_->GetResponseAsString(&report_id)) {
      LOG(WARNING) << "WebRTC event log completed, but report ID unknown.";
    } else {
      // TODO(crbug.com/775415): Remove this when chrome://webrtc-logs updated.
      VLOG(1) << "WebRTC event log successfully uploaded: " << report_id;
    }
  } else {
    LOG(WARNING) << "WebRTC event log upload failed.";
  }

  url_fetcher_.reset();  // Explicitly maintain determinant.

  ReportResult(upload_successful);
}

void WebRtcEventLogUploaderImpl::ReportResult(bool result) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // * If the upload was successful, the file is no longer needed.
  // * If the upload failed, we don't want to retry, because we run the risk of
  //   uploading significant amounts of data once again, only for the upload to
  //   fail again after (as an example) wasting 50MBs of upload bandwidth.
  // * If the file was not found, this will simply have no effect (other than
  //   to LOG() an error).
  // TODO(crbug.com/775415): Provide refined retrial behavior.
  DeleteLogFile();

  observer_->OnWebRtcEventLogUploadComplete(log_file_.path, result);
}

void WebRtcEventLogUploaderImpl::DeleteLogFile() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  const bool deletion_successful =
      base::DeleteFile(log_file_.path, /*recursive=*/false);
  if (!deletion_successful) {
    // This is a somewhat serious (though unlikely) error, because now we'll
    // try to upload this file again next time Chrome launches.
    LOG(ERROR) << "Could not delete pending WebRTC event log file.";
  }
}
