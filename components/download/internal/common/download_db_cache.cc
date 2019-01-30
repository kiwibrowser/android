// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/download_db_cache.h"

#include "components/download/database/download_db.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/public/common/download_utils.h"

namespace download {

namespace {

bool GetFetchErrorBody(base::Optional<DownloadDBEntry> entry) {
  if (!entry)
    return false;

  if (!entry->download_info)
    return false;

  base::Optional<InProgressInfo>& in_progress_info =
      entry->download_info->in_progress_info;
  if (!in_progress_info)
    return false;

  return in_progress_info->fetch_error_body;
}

DownloadUrlParameters::RequestHeadersType GetRequestHeadersType(
    base::Optional<DownloadDBEntry> entry) {
  DownloadUrlParameters::RequestHeadersType ret;
  if (!entry)
    return ret;

  if (!entry->download_info)
    return ret;

  base::Optional<InProgressInfo>& in_progress_info =
      entry->download_info->in_progress_info;
  if (!in_progress_info)
    return ret;

  return in_progress_info->request_headers;
}

DownloadSource GetDownloadSource(base::Optional<DownloadDBEntry> entry) {
  if (!entry)
    return DownloadSource::UNKNOWN;

  if (!entry->download_info)
    return DownloadSource::UNKNOWN;

  base::Optional<UkmInfo>& ukm_info = entry->download_info->ukm_info;
  if (!ukm_info)
    return DownloadSource::UNKNOWN;

  return ukm_info->download_source;
}

void CleanUpInProgressEntry(DownloadDBEntry& entry) {
  if (!entry.download_info)
    return;

  base::Optional<InProgressInfo>& in_progress_info =
      entry.download_info->in_progress_info;
  if (!in_progress_info)
    return;

  if (in_progress_info->state == DownloadItem::DownloadState::IN_PROGRESS) {
    in_progress_info->state = DownloadItem::DownloadState::INTERRUPTED;
    in_progress_info->interrupt_reason =
        download::DOWNLOAD_INTERRUPT_REASON_CRASH;
  }
}

}  // namespace

DownloadDBCache::DownloadDBCache(std::unique_ptr<DownloadDB> download_db)
    : initialized_(false),
      download_db_(std::move(download_db)),
      weak_factory_(this) {}

DownloadDBCache::~DownloadDBCache() = default;

void DownloadDBCache::Initialize(InitializeCallback callback) {
  // TODO(qinmin): migrate all the data from InProgressCache into
  // |download_db_|.
  if (!initialized_) {
    download_db_->Initialize(
        base::BindOnce(&DownloadDBCache::OnDownloadDBInitialized,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::unique_ptr<std::vector<DownloadDBEntry>> entries;
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    entries->emplace_back(it->second);
  }
  std::move(callback).Run(std::move(entries));
}

base::Optional<DownloadDBEntry> DownloadDBCache::RetrieveEntry(
    const std::string& guid) {
  auto iter = entries_.find(guid);
  if (iter != entries_.end())
    return iter->second;
  return base::nullopt;
}

void DownloadDBCache::AddOrReplaceEntry(const DownloadDBEntry& entry) {
  if (!entry.download_info)
    return;
  const std::string& guid = entry.download_info->guid;
  base::Optional<DownloadDBEntry> current = RetrieveEntry(guid);
  if (!current || entry != current.value()) {
    entries_.emplace(guid, entry);
    download_db_->AddOrReplace(entry);
  }
}

void DownloadDBCache::RemoveEntry(const std::string& guid) {
  entries_.erase(guid);
  if (download_db_)
    download_db_->Remove(guid);
}

void DownloadDBCache::OnDownloadUpdated(DownloadItem* download) {
  // TODO(crbug.com/778425): Properly handle fail/resume/retry for downloads
  // that are in the INTERRUPTED state for a long time.
  if (!download_db_)
    return;

  base::Optional<DownloadDBEntry> current = RetrieveEntry(download->GetGuid());
  bool fetch_error_body = GetFetchErrorBody(current);
  DownloadUrlParameters::RequestHeadersType request_header_type =
      GetRequestHeadersType(current);
  DownloadSource download_source = GetDownloadSource(current);
  // TODO(http://crbug.com/850990): Throttle the database updates, it is very
  // costly.
  DownloadDBEntry entry = CreateDownloadDBEntryFromItem(
      *download, download_source, fetch_error_body, request_header_type);
  AddOrReplaceEntry(entry);
}

void DownloadDBCache::OnDownloadRemoved(DownloadItem* download) {
  RemoveEntry(download->GetGuid());
}

void DownloadDBCache::OnDownloadDBInitialized(InitializeCallback callback,
                                              bool success) {
  if (success) {
    download_db_->LoadEntries(
        base::BindOnce(&DownloadDBCache::OnDownloadDBEntriesLoaded,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
  // TODO(qinmin): Recreate the database if |success| is false.
  // http://crbug.com/847661.
}

void DownloadDBCache::OnDownloadDBEntriesLoaded(
    InitializeCallback callback,
    bool success,
    std::unique_ptr<std::vector<DownloadDBEntry>> entries) {
  if (success) {
    initialized_ = true;
    for (auto& entry : *entries) {
      CleanUpInProgressEntry(entry);
      entries_.emplace(entry.download_info->guid, entry);
    }
    std::move(callback).Run(std::move(entries));
  }
  // TODO(qinmin): Recreate the database if |success| is false.
  // http://crbug.com/847661.
}

}  //  namespace download
