
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/policy/weekly_time/weekly_time.h"
#include "chrome/browser/chromeos/policy/weekly_time/weekly_time_interval.h"

namespace policy {
namespace weekly_time_utils {

// Put time in milliseconds which is added to local time to get GMT time to
// |offset| considering daylight from |clock|. Return true if there was no
// error.
bool GetOffsetFromTimezoneToGmt(const std::string& timezone,
                                base::Clock* clock,
                                int* offset);

// Convert time intervals from |timezone| to GMT timezone.
std::vector<WeeklyTimeInterval> ConvertIntervalsToGmt(
    const std::vector<WeeklyTimeInterval>& intervals,
    base::Clock* clock,
    const std::string& timezone);

// Return duration till next weekly time interval.
base::TimeDelta GetDeltaTillNextTimeInterval(
    const WeeklyTime& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals);

}  // namespace weekly_time_utils
}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_TIME_UTILS_H_
