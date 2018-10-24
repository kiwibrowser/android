// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/iwlwifi_dump_log_source.h"

#include "base/files/file_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

namespace system_logs {

namespace {

constexpr char kIwlwifiDumpKey[] = "iwlwifi_dump";
constexpr char kIwlwifiDumpLocation[] = "/var/log/last_iwlwifi_dump";

std::unique_ptr<SystemLogsResponse> CheckExistenceOnBlockingTaskRunner() {
  auto result = std::make_unique<SystemLogsResponse>();
  if (base::PathExists(base::FilePath(kIwlwifiDumpLocation))) {
    result->emplace(
        kIwlwifiDumpKey,
        l10n_util::GetStringUTF8(IDS_FEEDBACK_IWLWIFI_DEBUG_DUMP_EXPLAINER));
  }
  return result;
}

std::unique_ptr<SystemLogsResponse> ReadDumpOnBlockingTaskRunner() {
  auto result = std::make_unique<SystemLogsResponse>();
  std::string contents;
  if (base::ReadFileToString(base::FilePath(kIwlwifiDumpLocation), &contents))
    result->emplace(kIwlwifiDumpKey, std::move(contents));
  return result;
}

}  // namespace

IwlwifiDumpChecker::IwlwifiDumpChecker()
    : SystemLogsSource("IwlwifiDumpChecker") {}

IwlwifiDumpChecker::~IwlwifiDumpChecker() = default;

void IwlwifiDumpChecker::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits(base::MayBlock(), base::TaskPriority::BACKGROUND),
      base::BindOnce(&CheckExistenceOnBlockingTaskRunner),
      base::BindOnce(std::move(callback)));
}

IwlwifiDumpLogSource::IwlwifiDumpLogSource()
    : SystemLogsSource("IwlwifiDump") {}

IwlwifiDumpLogSource::~IwlwifiDumpLogSource() = default;

void IwlwifiDumpLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      base::TaskTraits(base::MayBlock(), base::TaskPriority::BACKGROUND),
      base::BindOnce(&ReadDumpOnBlockingTaskRunner),
      base::BindOnce(std::move(callback)));
}

bool ContainsIwlwifiLogs(FeedbackCommon::SystemLogsMap* sys_logs) {
  return sys_logs->count(kIwlwifiDumpKey);
}

void MergeIwlwifiLogs(
    std::unique_ptr<FeedbackCommon::SystemLogsMap> original_sys_logs,
    system_logs::SysLogsFetcherCallback callback,
    std::unique_ptr<system_logs::SystemLogsResponse> fetched_iwlwifi_response) {
  if (fetched_iwlwifi_response->count(kIwlwifiDumpKey)) {
    (*original_sys_logs)[kIwlwifiDumpKey] =
        std::move(fetched_iwlwifi_response->at(kIwlwifiDumpKey));
  }

  std::move(callback).Run(std::move(original_sys_logs));
}

}  // namespace system_logs
