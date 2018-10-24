// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_

#include <memory>

#include "base/values.h"
#include "chrome/browser/chromeos/policy/weekly_time/weekly_time.h"
#include "components/policy/proto/chrome_device_policy.pb.h"

namespace policy {

// Represents non-empty time interval [start, end) between two weekly times.
// Interval can be wrapped across the end of the week.
// Interval is empty if start = end. Empty intervals aren't allowed.
class WeeklyTimeInterval {
 public:
  WeeklyTimeInterval(const WeeklyTime& start, const WeeklyTime& end);

  WeeklyTimeInterval(const WeeklyTimeInterval& rhs);

  WeeklyTimeInterval& operator=(const WeeklyTimeInterval& rhs);

  // Return DictionaryValue in format:
  // { "start" : WeeklyTime,
  //   "end" : WeeklyTime }
  // WeeklyTime dictionary format:
  // { "day_of_week" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday,
  // etc.)
  //   "time" : int # in milliseconds from the beginning of the day.
  // }
  std::unique_ptr<base::DictionaryValue> ToValue() const;

  // Check if |w| is in [WeeklyTimeIntervall.start, WeeklyTimeInterval.end).
  // |end| time is always after |start| time. It's possible because week time is
  // cyclic. (i.e. [Friday 17:00, Monday 9:00) )
  bool Contains(const WeeklyTime& w) const;

  // Return time interval made from WeeklyTimeIntervalProto structure. Return
  // nullptr if the proto contains an invalid interval.
  static std::unique_ptr<WeeklyTimeInterval> ExtractFromProto(
      const enterprise_management::WeeklyTimeIntervalProto& container);

  WeeklyTime start() const { return start_; }

  WeeklyTime end() const { return end_; }

 private:
  WeeklyTime start_;
  WeeklyTime end_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_WEEKLY_TIME_WEEKLY_TIME_INTERVAL_H_
