// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_autoupdate_time_restrictions_proto_parser.h"

#include "chrome/browser/chromeos/policy/weekly_time/weekly_time_interval.h"

namespace em = enterprise_management;

namespace policy {

std::vector<WeeklyTimeInterval>
ExtractDisallowedIntervalsFromAutoUpdateSettingsProto(
    const em::AutoUpdateSettingsProto& container) {
  std::vector<WeeklyTimeInterval> intervals;
  for (const auto& entry : container.disallowed_time_intervals()) {
    auto interval = WeeklyTimeInterval::ExtractFromProto(entry);
    if (interval)
      intervals.push_back(*interval);
  }
  return intervals;
}

std::unique_ptr<base::ListValue> AutoUpdateDisallowedTimeIntervalsToValue(
    const em::AutoUpdateSettingsProto& container) {
  if (container.disallowed_time_intervals_size() == 0)
    return nullptr;
  std::vector<WeeklyTimeInterval> intervals =
      ExtractDisallowedIntervalsFromAutoUpdateSettingsProto(container);
  if (intervals.empty())
    return nullptr;
  auto time_restrictions = std::make_unique<base::ListValue>();
  for (const auto& interval : intervals)
    time_restrictions->Append(interval.ToValue());
  return time_restrictions;
}

}  // namespace policy
