// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/weekly_time/time_utils.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {
namespace weekly_time_utils {
namespace {
constexpr base::TimeDelta kWeek = base::TimeDelta::FromDays(7);
}  // namespace

bool GetOffsetFromTimezoneToGmt(const std::string& timezone,
                                base::Clock* clock,
                                int* offset) {
  auto zone = base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8(timezone)));
  if (*zone == icu::TimeZone::getUnknown()) {
    LOG(ERROR) << "Unsupported timezone: " << timezone;
    return false;
  }
  // Time in milliseconds which is added to GMT to get local time.
  int gmt_offset = zone->getRawOffset();
  // Time in milliseconds which is added to local standard time to get local
  // wall clock time.
  int dst_offset = zone->getDSTSavings();
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::GregorianCalendar> gregorian_calendar =
      std::make_unique<icu::GregorianCalendar>(*zone, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Gregorian calendar error = " << u_errorName(status);
    return false;
  }
  UDate cur_date = static_cast<UDate>(clock->Now().ToDoubleT() *
                                      base::Time::kMillisecondsPerSecond);
  status = U_ZERO_ERROR;
  gregorian_calendar->setTime(cur_date, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Gregorian calendar set time error = " << u_errorName(status);
    return false;
  }
  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar->inDaylightTime(status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Daylight time error = " << u_errorName(status);
    return false;
  }
  if (day_light)
    gmt_offset += dst_offset;
  // -|gmt_offset| is time which is added to local time to get GMT time.
  *offset = -gmt_offset;
  return true;
}

std::vector<WeeklyTimeInterval> ConvertIntervalsToGmt(
    const std::vector<WeeklyTimeInterval>& intervals,
    base::Clock* clock,
    const std::string& timezone) {
  int gmt_offset = 0;
  bool no_offset_error =
      GetOffsetFromTimezoneToGmt(timezone, clock, &gmt_offset);
  if (!no_offset_error)
    return {};
  std::vector<WeeklyTimeInterval> gmt_intervals;
  for (const auto& interval : intervals) {
    // |gmt_offset| is added to input time to get GMT time.
    auto gmt_start = interval.start().AddMilliseconds(gmt_offset);
    auto gmt_end = interval.end().AddMilliseconds(gmt_offset);
    gmt_intervals.push_back(WeeklyTimeInterval(gmt_start, gmt_end));
  }
  return gmt_intervals;
}

base::TimeDelta GetDeltaTillNextTimeInterval(
    const WeeklyTime& current_time,
    const std::vector<WeeklyTimeInterval>& weekly_time_intervals) {
  // Weekly intervals repeat every week, therefore the maximum duration till
  // next weekly interval is one week.
  base::TimeDelta till_next_interval = kWeek;
  for (const auto& interval : weekly_time_intervals) {
    till_next_interval = std::min(till_next_interval,
                                  current_time.GetDurationTo(interval.start()));
  }
  return till_next_interval;
}

}  // namespace weekly_time_utils
}  // namespace policy
