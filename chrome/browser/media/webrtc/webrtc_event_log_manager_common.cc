// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

#include <limits>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;
using LogFilesMap = std::map<WebRtcEventLogPeerConnectionKey, LogFile>;

const char kStartRemoteLoggingFailureFeatureDisabled[] = "Feature disabled.";
const char kStartRemoteLoggingFailureUnlimitedSizeDisallowed[] =
    "Unlimited size disallowed.";
const char kStartRemoteLoggingFailureMaxSizeTooLarge[] =
    "Excessively large max log size.";
const char kStartRemoteLoggingFailureMetadaTooLong[] =
    "Excessively long metadata.";
const char kStartRemoteLoggingFailureMaxSizeTooSmall[] = "Max size too small.";
const char kStartRemoteLoggingFailureUnknownOrInactivePeerConnection[] =
    "Unknown or inactive peer connection.";
const char kStartRemoteLoggingFailureAlreadyLogging[] = "Already logging.";
const char kStartRemoteLoggingFailureGeneric[] = "Unspecified error.";

const BrowserContextId kNullBrowserContextId =
    reinterpret_cast<BrowserContextId>(nullptr);

LogFile::LogFile(const base::FilePath& path,
                 base::File file,
                 size_t max_file_size_bytes)
    : path_(path),
      file_(std::move(file)),
      max_file_size_bytes_(max_file_size_bytes),
      file_size_bytes_(0) {}

LogFile::LogFile(LogFile&& other)
    : path_(std::move(other.path_)),
      file_(std::move(other.file_)),
      max_file_size_bytes_(other.max_file_size_bytes_),
      file_size_bytes_(other.file_size_bytes_) {}

LogFile::~LogFile() = default;

bool LogFile::MaxSizeReached() const {
  if (max_file_size_bytes_ == kWebRtcEventLogManagerUnlimitedFileSize) {
    return false;
  }
  DCHECK_LE(file_size_bytes_, max_file_size_bytes_);
  return file_size_bytes_ >= max_file_size_bytes_;
}

bool LogFile::Write(const std::string& message) {
  DCHECK_LE(message.length(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  // Observe the file size limit, if any. Note that base::File's interface does
  // not allow writing more than numeric_limits<int>::max() bytes at a time.
  int message_len = static_cast<int>(message.length());  // DCHECKed above.
  if (max_file_size_bytes_ != kWebRtcEventLogManagerUnlimitedFileSize) {
    DCHECK_LT(file_size_bytes_, max_file_size_bytes_);
    const bool size_will_wrap_around =
        file_size_bytes_ + message.length() < file_size_bytes_;
    const bool size_limit_will_be_exceeded =
        file_size_bytes_ + message.length() > max_file_size_bytes_;
    if (size_will_wrap_around || size_limit_will_be_exceeded) {
      return false;
    }
  }

  int written = file_.WriteAtCurrentPos(message.c_str(), message_len);
  if (written != message_len) {
    LOG(WARNING) << "WebRTC event log message couldn't be written to the "
                    "locally stored file in its entirety.";
    return false;
  }

  file_size_bytes_ += static_cast<size_t>(written);
  DCHECK(max_file_size_bytes_ == kWebRtcEventLogManagerUnlimitedFileSize ||
         file_size_bytes_ <= max_file_size_bytes_);

  return true;
}

void LogFile::Close() {
  file_.Flush();
  file_.Close();
}

void LogFile::Delete() {
  if (!base::DeleteFile(path_, /*recursive=*/false)) {
    LOG(ERROR) << "Failed to delete " << path_ << ".";
  }
}

BrowserContextId GetBrowserContextId(
    const content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return reinterpret_cast<BrowserContextId>(browser_context);
}

BrowserContextId GetBrowserContextId(int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* const host =
      content::RenderProcessHost::FromID(render_process_id);

  content::BrowserContext* const browser_context =
      host ? host->GetBrowserContext() : nullptr;

  return GetBrowserContextId(browser_context);
}

base::FilePath GetRemoteBoundWebRtcEventLogsDir(
    const base::FilePath& browser_context_dir) {
  const base::FilePath::CharType kRemoteBoundLogSubDirectory[] =
      FILE_PATH_LITERAL("webrtc_event_logs");
  return browser_context_dir.Append(kRemoteBoundLogSubDirectory);
}
