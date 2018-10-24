// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_IWLWIFI_DUMP_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_IWLWIFI_DUMP_LOG_SOURCE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// The classes here are used to attach debug dump information from
// Intel Wi-Fi NICs that will be produced when those NICs have issues
// such as firmware crashes. This information will be used to help
// diagnose Wi-Fi issues.

// This logs source is used to check for the existence of the Wi-Fi
// debug dump. It will place an explainer string in the system logs
// map if it finds the dump.
class IwlwifiDumpChecker : public SystemLogsSource {
 public:
  IwlwifiDumpChecker();
  ~IwlwifiDumpChecker() override;

  // system_logs::SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IwlwifiDumpChecker);
};

// Fetches information from the /var/log/last_iwlwifi_dump file, if
// the explainer string is present in the passed-in logs map.
class IwlwifiDumpLogSource : public SystemLogsSource {
 public:
  IwlwifiDumpLogSource();
  ~IwlwifiDumpLogSource() override;

  // system_logs::SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IwlwifiDumpLogSource);
};

// Checks to see if |sys_logs| contains the iwlwifi logs key.
bool ContainsIwlwifiLogs(FeedbackCommon::SystemLogsMap* sys_logs);

// This should be passed as a callback to the fetcher that will fetch logs
// from the IwlwifiDumpLogSource above. It will merge the
// |fetched_iwlwifi_response| into the |original_sys_logs| and call the
// |callback| with that result.
void MergeIwlwifiLogs(
    std::unique_ptr<FeedbackCommon::SystemLogsMap> original_sys_logs,
    system_logs::SysLogsFetcherCallback callback,
    std::unique_ptr<system_logs::SystemLogsResponse> fetched_iwlwifi_response);

}  // namespace system_logs

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_IWLWIFI_DUMP_LOG_SOURCE_H_
