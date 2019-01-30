// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Processor for the UsageTimeLimit policy. Used to determine the current state
// of the client, for example if it is locked and the reason why it may be
// locked.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_

#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"

namespace chromeos {
namespace usage_time_limit {
namespace internal {

enum class Weekday {
  kSunday = 0,
  kMonday,
  kTuesday,
  kWednesday,
  kThursday,
  kFriday,
  kSaturday,
  kCount,
};

struct TimeWindowLimitEntry {
  TimeWindowLimitEntry();

  bool IsOvernight() const;

  // Start time of time window limit. This is the distance from midnight.
  base::TimeDelta starts_at;
  // End time of time window limit. This is the distance from midnight.
  base::TimeDelta ends_at;
  // Last time this entry was updated.
  base::Time last_updated;
};

class TimeWindowLimit {
 public:
  TimeWindowLimit(const base::Value& window_limit_dict);
  ~TimeWindowLimit();
  TimeWindowLimit(TimeWindowLimit&&);
  TimeWindowLimit& operator=(TimeWindowLimit&&);

  std::unordered_map<Weekday, base::Optional<TimeWindowLimitEntry>> entries;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeWindowLimit);
};

struct TimeUsageLimitEntry {
  TimeUsageLimitEntry();

  base::TimeDelta usage_quota;
  base::Time last_updated;
};

class TimeUsageLimit {
 public:
  TimeUsageLimit(const base::Value& usage_limit_dict);
  ~TimeUsageLimit();
  TimeUsageLimit(TimeUsageLimit&&);
  TimeUsageLimit& operator=(TimeUsageLimit&&);

  std::unordered_map<Weekday, base::Optional<TimeUsageLimitEntry>> entries;
  base::TimeDelta resets_at;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimeUsageLimit);
};

class Override {
 public:
  enum class Action { kLock, kUnlock };

  Override(const base::Value& override_dict);
  ~Override();
  Override(Override&&);
  Override& operator=(Override&&);

  Action action;
  base::Time created_at;
  base::Optional<base::TimeDelta> duration;

 private:
  DISALLOW_COPY_AND_ASSIGN(Override);
};

// Retrieves the weekday from a time.
Weekday GetWeekday(base::Time time);

// Shifts the current weekday, if the value is po
Weekday WeekdayShift(Weekday current_day, int shift);

}  // namespace internal

enum class ActivePolicies {
  kNoActivePolicy,
  kOverride,
  kFixedLimit,
  kUsageLimit
};

struct State {
  // Whether the device is currently locked.
  bool is_locked = false;

  // Which policy is responsible for the current state.
  // If it is locked, one of [ override, fixed_limit, usage_limit ]
  // If it is not locked, one of [ no_active_policy, override ]
  ActivePolicies active_policy;

  // Whether time_usage_limit is currently active.
  bool is_time_usage_limit_enabled = false;

  // Remaining screen usage quota. Only available if
  // is_time_limit_enabled = true
  base::TimeDelta remaining_usage;

  // When the time usage limit started being enforced. Only available when
  // is_time_usage_limit_enabled = true and remaining_usage is 0, which means
  // that the time usage limit is enforced, and therefore should have a start
  // time.
  base::Time time_usage_limit_started;

  // Next epoch time that time limit state could change. This could be the
  // start time of the next fixed window limit, the end time of the current
  // fixed limit, the earliest time a usage limit could be reached, or the
  // next time when screen time will start.
  base::Time next_state_change_time;

  // The policy that will be active in the next state.
  ActivePolicies next_state_active_policy;

  // Last time the state changed.
  base::Time last_state_changed = base::Time();
};

// Returns the current state of the user session with the given usage time limit
// policy.
State GetState(const std::unique_ptr<base::DictionaryValue>& time_limit,
               const base::TimeDelta& used_time,
               const base::Time& usage_timestamp,
               const base::Time& current_time,
               const base::Optional<State>& previous_state);

// Ruturns the expected time that the used time stored should be reseted.
base::Time GetExpectedResetTime(
    const std::unique_ptr<base::DictionaryValue>& time_limit,
    base::Time current_time);

}  // namespace usage_time_limit
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_USAGE_TIME_LIMIT_PROCESSOR_H_
