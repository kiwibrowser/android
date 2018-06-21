// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usage_time_limit_processor.h"

#include "base/strings/string_number_conversions.h"

namespace chromeos {
namespace usage_time_limit {
namespace internal {
namespace {

constexpr char kOverride[] = "overrides";
constexpr char kOverrideAction[] = "action";
constexpr char kOverrideActionCreatedAt[] = "created_at_millis";
constexpr char kOverrideActionDurationMins[] = "duration_mins";
constexpr char kOverrideActionLock[] = "LOCK";
constexpr char kOverrideActionSpecificData[] = "action_specific_data";
constexpr char kTimeLimitLastUpdatedAt[] = "last_updated_millis";
constexpr char kTimeWindowLimit[] = "time_window_limit";
constexpr char kTimeUsageLimit[] = "time_usage_limit";
constexpr char kUsageLimitResetAt[] = "reset_at";
constexpr char kUsageLimitUsageQuota[] = "usage_quota_mins";
constexpr char kWindowLimitEntries[] = "entries";
constexpr char kWindowLimitEntryEffectiveDay[] = "effective_day";
constexpr char kWindowLimitEntryEndsAt[] = "ends_at";
constexpr char kWindowLimitEntryStartsAt[] = "starts_at";
constexpr char kWindowLimitEntryTimeHour[] = "hour";
constexpr char kWindowLimitEntryTimeMinute[] = "minute";
constexpr const char* kTimeLimitWeekdays[] = {
    "sunday",   "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"};

// Whether a timestamp is inside a window.
bool ContainsTime(base::Time start, base::Time end, base::Time now) {
  return now >= start && now < end;
}

// Returns true when a < b. When b is null, this returns false.
bool IsBefore(base::Time a, base::Time b) {
  return b.is_null() || a < b;
}

// The UTC midnight for a timestamp.
base::Time UTCMidnight(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time out_time;
  if (base::Time::FromUTCExploded(exploded, &out_time))
    return out_time;
  return time;
}

// Helper class to process the UsageTimeLimit policy.
class UsageTimeLimitProcessor {
 public:
  UsageTimeLimitProcessor(
      base::Optional<internal::TimeWindowLimit> time_window_limit,
      base::Optional<internal::TimeUsageLimit> time_usage_limit,
      base::Optional<internal::Override> override,
      const base::TimeDelta& used_time,
      const base::Time& usage_timestamp,
      const base::Time& current_time,
      const base::Optional<State>& previous_state);

  ~UsageTimeLimitProcessor() = default;

  // Current user's session state.
  State GetState();

  // Expected time when the user's usage quota should be reseted.
  base::Time GetExpectedResetTime();

 private:
  // Get the active time window limit.
  base::Optional<internal::TimeWindowLimitEntry> GetActiveTimeWindowLimit();

  // Get the active time usage limit.
  base::Optional<internal::TimeUsageLimitEntry> GetActiveTimeUsageLimit();

  // Get the enabled time usage limit.
  base::Optional<internal::TimeUsageLimitEntry> GetEnabledTimeUsageLimit();

  // Returns the duration of all the consuctive time window limit starting at
  // the given weekday.
  base::TimeDelta GetConsecutiveTimeWindowLimitDuration(
      internal::Weekday weekday);

  // Whether there is a valid override.
  bool HasActiveOverride();

  // Whether the user's session should be locked.
  bool IsLocked();

  // Which policy is currently active.
  ActivePolicies GetActivePolicy();

  // Expected time when the state will change.
  base::Time GetNextStateChangeTime(ActivePolicies* out_next_active);

  // Whether the time window limit defined in the given weekday is overridden.
  bool IsWindowLimitOverridden(internal::Weekday weekday);

  // Whether the time usage limit defined in the given weekday is overridden.
  bool IsUsageLimitOverridden(internal::Weekday weekday);

  // When the lock override should reset.
  base::TimeDelta LockOverrideResetTime();

  // When the usage limit should reset the usage quota.
  base::TimeDelta UsageLimitResetTime();

  // Checks if the time window limit entry for the current weekday is active.
  bool IsTodayTimeWindowLimitActive();

  // Converts the policy time, which is a delta from midnight, to a timestamp.
  // Since this is done based on the current time, a shift in days param is
  // available.
  base::Time ConvertPolicyTime(base::TimeDelta policy_time, int shift_in_days);

  // The policy time window limit object.
  base::Optional<internal::TimeWindowLimit> time_window_limit;

  // The policy time usage limit object.
  base::Optional<internal::TimeUsageLimit> time_usage_limit;

  // The policy override object.
  base::Optional<internal::Override> override;

  // How long the user has used the device.
  const base::TimeDelta used_time;

  // When the used_time data was collected.
  const base::Time usage_timestamp;

  // The current time, not necessarily equal to usage_timestamp.
  const base::Time current_time;

  // Current weekday, extracted from current time.
  internal::Weekday current_weekday;

  // The previous state calculated by this class.
  const base::Optional<State>& previous_state;

  // The active time window limit. If this is set, it means that the user
  // session should be locked, in other words, there is a time window limit set
  // for the current day, the current time is inside that window and no unlock
  // override is preventing it to be locked.
  base::Optional<internal::TimeWindowLimitEntry> active_time_window_limit;

  // The active time usage limit. If this is set, it means that the user session
  // should be locked, in other words, there is a time usage limit set for the
  // current day, the user has used all their usage quota and no unlock override
  // is preventing it to be locked.
  base::Optional<internal::TimeUsageLimitEntry> active_time_usage_limit;

  // If this is set, it means that there is a time usage limit set for today,
  // but it is not necessarily active. It could be inactive either because the
  // user haven't used all their quota or because there is an unlock override
  // active.
  base::Optional<internal::TimeUsageLimitEntry> enabled_time_usage_limit;

  // Whether there is a window limit overridden.
  bool overridden_window_limit = false;

  // Whether there is a usage limit overridden.
  bool overridden_usage_limit = false;
};

UsageTimeLimitProcessor::UsageTimeLimitProcessor(
    base::Optional<internal::TimeWindowLimit> time_window_limit,
    base::Optional<internal::TimeUsageLimit> time_usage_limit,
    base::Optional<internal::Override> override,
    const base::TimeDelta& used_time,
    const base::Time& usage_timestamp,
    const base::Time& current_time,
    const base::Optional<State>& previous_state)
    : time_window_limit(std::move(time_window_limit)),
      time_usage_limit(std::move(time_usage_limit)),
      override(std::move(override)),
      used_time(used_time),
      usage_timestamp(usage_timestamp),
      current_time(current_time),
      current_weekday(internal::GetWeekday(current_time)),
      previous_state(previous_state),
      enabled_time_usage_limit(GetEnabledTimeUsageLimit()) {
  // This will also set overridden_window_limit to true if applicable.
  active_time_window_limit = GetActiveTimeWindowLimit();
  // This will also sets overridden_usage_limit to true if applicable.
  active_time_usage_limit = GetActiveTimeUsageLimit();
}

base::Time UsageTimeLimitProcessor::GetExpectedResetTime() {
  base::TimeDelta delta_from_midnight =
      current_time - UTCMidnight(current_time);
  int shift_in_days = 1;
  if (delta_from_midnight < UsageLimitResetTime())
    shift_in_days = 0;
  return ConvertPolicyTime(UsageLimitResetTime(), shift_in_days);
}

State UsageTimeLimitProcessor::GetState() {
  State state;
  state.is_locked = IsLocked();
  state.active_policy = GetActivePolicy();

  // Time usage limit is enabled if there is an entry for the current day and it
  // is not overridden.
  if (enabled_time_usage_limit || active_time_usage_limit) {
    state.is_time_usage_limit_enabled = true;
    state.remaining_usage =
        std::max(enabled_time_usage_limit->usage_quota - used_time,
                 base::TimeDelta::FromMinutes(0));
  }

  const base::TimeDelta delta_zero = base::TimeDelta::FromMinutes(0);
  // Time usage limit started when the usage quota ends.
  if ((previous_state && previous_state->remaining_usage > delta_zero &&
       state.remaining_usage <= delta_zero) ||
      (!previous_state && state.remaining_usage <= delta_zero)) {
    state.time_usage_limit_started = usage_timestamp;
  } else if (previous_state && previous_state->remaining_usage <= delta_zero) {
    state.time_usage_limit_started = previous_state->time_usage_limit_started;
  }

  state.next_state_change_time =
      GetNextStateChangeTime(&state.next_state_active_policy);

  if (!previous_state) {
    return state;
  }

  if (previous_state->is_locked == state.is_locked &&
      previous_state->active_policy == state.active_policy) {
    state.last_state_changed = previous_state->last_state_changed;
    return state;
  }

  state.last_state_changed = current_time;

  return state;
}

base::TimeDelta UsageTimeLimitProcessor::GetConsecutiveTimeWindowLimitDuration(
    internal::Weekday weekday) {
  base::TimeDelta duration = base::TimeDelta::FromMinutes(0);
  base::Optional<internal::TimeWindowLimitEntry> current_day_entry =
      time_window_limit->entries[weekday];

  if (!time_window_limit || !current_day_entry)
    return duration;

  // Iterate throught entries as long as they are consecutive, or overlap.
  base::TimeDelta last_entry_end = current_day_entry->starts_at;
  for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
    base::Optional<internal::TimeWindowLimitEntry> window_limit_entry =
        time_window_limit->entries[internal::WeekdayShift(weekday, i)];

    // It is not consecutive.
    if (!window_limit_entry || window_limit_entry->starts_at > last_entry_end)
      break;

    if (window_limit_entry->IsOvernight()) {
      duration += base::TimeDelta(base::TimeDelta::FromHours(24) -
                                  window_limit_entry->starts_at) +
                  base::TimeDelta(window_limit_entry->ends_at);
    } else {
      duration += base::TimeDelta(window_limit_entry->ends_at -
                                  window_limit_entry->starts_at);
      // This entry is not overnight, so the next one cannot be a consecutive
      // window.
      break;
    }
  }

  return duration;
}

bool UsageTimeLimitProcessor::IsWindowLimitOverridden(
    internal::Weekday weekday) {
  if (!time_window_limit || !override ||
      override->action == internal::Override::Action::kLock) {
    return false;
  }

  base::Optional<internal::TimeWindowLimitEntry> window_limit_entry =
      time_window_limit->entries[weekday];

  // If the time window limit has been updated since the override, it doesn't
  // take effect.
  if (!window_limit_entry ||
      window_limit_entry->last_updated > override->created_at)
    return false;

  int days_behind = 0;
  for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
    if (internal::WeekdayShift(weekday, i) == current_weekday) {
      days_behind = i;
      break;
    }
  }

  base::Time window_limit_start =
      ConvertPolicyTime(window_limit_entry->starts_at, -days_behind);
  base::Time window_limit_end =
      window_limit_start + GetConsecutiveTimeWindowLimitDuration(weekday);

  return ContainsTime(window_limit_start, window_limit_end,
                      override->created_at);
}

bool UsageTimeLimitProcessor::IsUsageLimitOverridden(
    internal::Weekday weekday) {
  if (!override || override->action == internal::Override::Action::kLock)
    return false;

  if (!time_usage_limit || !previous_state)
    return false;

  base::Optional<internal::TimeUsageLimitEntry> usage_limit_entry =
      time_usage_limit->entries[weekday];

  // If the time usage limit has been updated since the override, it doesn't
  // take effect.
  if (!usage_limit_entry ||
      usage_limit_entry->last_updated > override->created_at)
    return false;

  bool usage_limit_enforced_previously =
      previous_state->is_time_usage_limit_enabled &&
      previous_state->remaining_usage <= base::TimeDelta::FromMinutes(0);
  bool override_created_after_usage_limit_start =
      override->created_at > previous_state->time_usage_limit_started;
  return usage_limit_enforced_previously &&
         override_created_after_usage_limit_start;
}

base::Optional<internal::TimeWindowLimitEntry>
UsageTimeLimitProcessor::GetActiveTimeWindowLimit() {
  if (!time_window_limit)
    return base::nullopt;

  internal::Weekday previous_weekday =
      internal::WeekdayShift(current_weekday, -1);
  base::Optional<internal::TimeWindowLimitEntry> previous_day_entry =
      time_window_limit->entries[previous_weekday];

  // Active time window limit that started on the previous day.
  base::Optional<internal::TimeWindowLimitEntry> previous_day_active_entry;
  if (previous_day_entry && previous_day_entry->IsOvernight()) {
    base::Time limit_start =
        ConvertPolicyTime(previous_day_entry->starts_at, -1);
    base::Time limit_end = ConvertPolicyTime(previous_day_entry->ends_at, 0);

    if (ContainsTime(limit_start, limit_end, current_time)) {
      if (IsWindowLimitOverridden(previous_weekday)) {
        overridden_window_limit = true;
      } else {
        previous_day_active_entry = previous_day_entry;
      }
    }
  }

  base::Optional<internal::TimeWindowLimitEntry> current_day_entry =
      time_window_limit->entries[current_weekday];

  // Active time window limit that started today.
  base::Optional<internal::TimeWindowLimitEntry> current_day_active_entry;
  if (current_day_entry) {
    base::Time limit_start = ConvertPolicyTime(current_day_entry->starts_at, 0);
    base::Time limit_end = ConvertPolicyTime(
        current_day_entry->ends_at, current_day_entry->IsOvernight() ? 1 : 0);

    if (ContainsTime(limit_start, limit_end, current_time)) {
      if (IsWindowLimitOverridden(current_weekday)) {
        overridden_window_limit = true;
      } else {
        current_day_active_entry = current_day_entry;
      }
    }
  }

  if (current_day_active_entry && previous_day_active_entry) {
    // If two windows overlap and are active now we must return the one that
    // ends later.
    if (current_day_active_entry->IsOvernight() ||
        current_day_active_entry->ends_at >
            previous_day_active_entry->ends_at) {
      return current_day_active_entry;
    }
    return previous_day_active_entry;
  }

  if (current_day_active_entry)
    return current_day_active_entry;

  return previous_day_active_entry;
}

base::Optional<internal::TimeUsageLimitEntry>
UsageTimeLimitProcessor::GetEnabledTimeUsageLimit() {
  if (!time_usage_limit)
    return base::nullopt;

  internal::Weekday current_usage_limit_day =
      current_time > ConvertPolicyTime(UsageLimitResetTime(), 0)
          ? current_weekday
          : internal::WeekdayShift(current_weekday, -1);
  return time_usage_limit->entries[current_usage_limit_day];
}

base::Optional<internal::TimeUsageLimitEntry>
UsageTimeLimitProcessor::GetActiveTimeUsageLimit() {
  if (!time_usage_limit)
    return base::nullopt;

  internal::Weekday current_usage_limit_day =
      current_time > ConvertPolicyTime(UsageLimitResetTime(), 0)
          ? current_weekday
          : internal::WeekdayShift(current_weekday, -1);

  base::Optional<internal::TimeUsageLimitEntry> current_usage_limit =
      GetEnabledTimeUsageLimit();

  if (IsUsageLimitOverridden(current_usage_limit_day))
    return base::nullopt;

  if (current_usage_limit && used_time >= current_usage_limit->usage_quota)
    return current_usage_limit;

  return base::nullopt;
}

bool UsageTimeLimitProcessor::HasActiveOverride() {
  if (!override)
    return false;

  if (overridden_window_limit || overridden_usage_limit)
    return true;

  if (!overridden_usage_limit && !overridden_window_limit &&
      override->action == internal::Override::Action::kLock)
    return true;

  return false;
}

bool UsageTimeLimitProcessor::IsLocked() {
  return active_time_usage_limit || active_time_window_limit ||
         (HasActiveOverride() &&
          override->action == internal::Override::Action::kLock);
}

ActivePolicies UsageTimeLimitProcessor::GetActivePolicy() {
  if (active_time_window_limit)
    return ActivePolicies::kFixedLimit;

  if (active_time_usage_limit)
    return ActivePolicies::kUsageLimit;

  if (HasActiveOverride())
    return ActivePolicies::kOverride;

  return ActivePolicies::kNoActivePolicy;
}

base::Time UsageTimeLimitProcessor::GetNextStateChangeTime(
    ActivePolicies* out_next_active) {
  base::Time next_change;
  *out_next_active = ActivePolicies::kNoActivePolicy;

  // Time when the time_window_limit ends. Only available if there is an
  // active time window limit.
  base::Time active_time_window_limit_ends;
  if (active_time_window_limit) {
    base::TimeDelta window_limit_duration =
        IsTodayTimeWindowLimitActive()
            ? GetConsecutiveTimeWindowLimitDuration(current_weekday)
            : GetConsecutiveTimeWindowLimitDuration(
                  internal::WeekdayShift(current_weekday, -1));
    active_time_window_limit_ends =
        ConvertPolicyTime(active_time_window_limit->starts_at,
                          IsTodayTimeWindowLimitActive() ? 0 : -1) +
        window_limit_duration;
  }

  // Next time when the usage quota will be reset.
  base::Time next_usage_quota_reset;
  bool has_reset_today =
      current_time - UTCMidnight(current_time) >= UsageLimitResetTime();
  next_usage_quota_reset =
      ConvertPolicyTime(UsageLimitResetTime(), has_reset_today ? 1 : 0);

  // Check when next time window limit starts.
  if (time_window_limit) {
    internal::Weekday start_day = current_weekday;
    if (IsTodayTimeWindowLimitActive())
      start_day = internal::WeekdayShift(start_day, 1);

    // Search a time window limit in the next following days.
    for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
      base::Optional<internal::TimeWindowLimitEntry> entry =
          time_window_limit.value()
              .entries[internal::WeekdayShift(start_day, i)];
      if (entry) {
        int shift = start_day == current_weekday ? 0 : 1;
        base::Time start_time = ConvertPolicyTime(entry->starts_at, i + shift);
        if (IsBefore(start_time, next_change)) {
          next_change = start_time;
          *out_next_active = ActivePolicies::kFixedLimit;
        }
        break;
      }
    }
  }

  // Minimum time when the time usage quota could end. Not calculated when
  // time usage limit has already finished. If there is no active time usage
  // limit on the current day, we search on the following days.
  if (time_usage_limit && !active_time_usage_limit && !overridden_usage_limit &&
      !active_time_window_limit) {
    // If there is an active time usage, we just look when it would lock the
    // session if the user don't stop using it.
    if (enabled_time_usage_limit) {
      base::Time quota_ends =
          current_time + (enabled_time_usage_limit->usage_quota - used_time);
      if (IsBefore(quota_ends, next_change)) {
        next_change = quota_ends;
        *out_next_active = ActivePolicies::kUsageLimit;
      }
    } else {
      // Look for the next time usage, and calculate when the minimum time it
      // could end.
      for (int i = 0; i < static_cast<int>(internal::Weekday::kCount); i++) {
        base::Optional<internal::TimeUsageLimitEntry> usage_limit_entry =
            time_usage_limit
                ->entries[internal::WeekdayShift(current_weekday, i)];
        if (usage_limit_entry) {
          base::Time quota_ends = ConvertPolicyTime(UsageLimitResetTime(), i) +
                                  usage_limit_entry->usage_quota;
          if (IsBefore(quota_ends, next_change)) {
            next_change = quota_ends;
            *out_next_active = ActivePolicies::kUsageLimit;
          }
          break;
        }
      }
    }
  }

  // When the current active time window limit ends.
  if (active_time_window_limit) {
    if (IsBefore(active_time_window_limit_ends, next_change)) {
      next_change = active_time_window_limit_ends;
      if (active_time_usage_limit &&
          used_time >= active_time_usage_limit->usage_quota &&
          active_time_window_limit_ends < next_usage_quota_reset) {
        *out_next_active = ActivePolicies::kUsageLimit;
      } else {
        *out_next_active = ActivePolicies::kNoActivePolicy;
      }
    }
  }

  // When the usage quota resets. Only calculated if there is an enforced time
  // usage limit, and when it ends no other policy would be active.
  if (active_time_usage_limit &&
      (!active_time_window_limit ||
       active_time_window_limit->ends_at < UsageLimitResetTime())) {
    if (IsBefore(next_usage_quota_reset, next_change)) {
      next_change = next_usage_quota_reset;
      *out_next_active = ActivePolicies::kNoActivePolicy;
    }
  }

  // When a lock override will become inactive. Lock overrides are disabled at
  // the same time as time usage limit resets.
  if (HasActiveOverride() &&
      override->action == internal::Override::Action::kLock) {
    // Whether this lock override was created after today's reset.
    bool lock_after_reset = override->created_at >
                            UTCMidnight(current_time) + LockOverrideResetTime();
    base::Time lock_end =
        ConvertPolicyTime(LockOverrideResetTime(), lock_after_reset ? 1 : 0);

    if (IsBefore(lock_end, next_change)) {
      next_change = lock_end;
      if (active_time_window_limit &&
          active_time_window_limit_ends > next_usage_quota_reset) {
        *out_next_active = ActivePolicies::kFixedLimit;
      } else {
        *out_next_active = ActivePolicies::kNoActivePolicy;
      }
    }
  }
  return next_change;
}

bool UsageTimeLimitProcessor::IsTodayTimeWindowLimitActive() {
  if (!time_window_limit)
    return false;

  base::Optional<internal::TimeWindowLimitEntry> today_window_limit =
      time_window_limit->entries[current_weekday];
  base::Optional<internal::TimeWindowLimitEntry> yesterday_window_limit =
      time_window_limit.value()
          .entries[internal::WeekdayShift(current_weekday, -1)];

  if ((active_time_window_limit || overridden_window_limit) &&
      (!yesterday_window_limit ||
       yesterday_window_limit->ends_at < today_window_limit->ends_at)) {
    return true;
  }
  return false;
}

base::TimeDelta UsageTimeLimitProcessor::UsageLimitResetTime() {
  if (time_usage_limit)
    return time_usage_limit->resets_at;
  return base::TimeDelta::FromMinutes(0);
}

base::TimeDelta UsageTimeLimitProcessor::LockOverrideResetTime() {
  // The default behavior is to stop enforcing the lock override at the same
  // time as the time usage limit resets.
  return UsageLimitResetTime();
}

base::Time UsageTimeLimitProcessor::ConvertPolicyTime(
    base::TimeDelta policy_time,
    int shift_in_days) {
  return UTCMidnight(current_time) + base::TimeDelta::FromDays(shift_in_days) +
         policy_time;
}

}  // namespace

// Transforms the time dictionary sent on the UsageTimeLimit policy to a
// TimeDelta, that represents the distance from midnight.
base::TimeDelta ValueToTimeDelta(const base::Value* policy_time) {
  int hour = policy_time->FindKey(kWindowLimitEntryTimeHour)->GetInt();
  int minute = policy_time->FindKey(kWindowLimitEntryTimeMinute)->GetInt();
  return base::TimeDelta::FromMinutes(hour * 60 + minute);
}

// Transforms weekday strings into the Weekday enum.
Weekday GetWeekday(std::string weekday) {
  std::transform(weekday.begin(), weekday.end(), weekday.begin(), ::tolower);
  for (int i = 0; i < static_cast<int>(Weekday::kCount); i++) {
    if (weekday == kTimeLimitWeekdays[i]) {
      return static_cast<Weekday>(i);
    }
  }

  LOG(ERROR) << "Unexpected weekday " << weekday;
  return Weekday::kSunday;
}

TimeWindowLimitEntry::TimeWindowLimitEntry() = default;

bool TimeWindowLimitEntry::IsOvernight() const {
  return ends_at < starts_at;
}

TimeWindowLimit::TimeWindowLimit(const base::Value& window_limit_dict) {
  if (!window_limit_dict.FindKey(kWindowLimitEntries))
    return;

  for (const base::Value& entry_dict :
       window_limit_dict.FindKey(kWindowLimitEntries)->GetList()) {
    const base::Value* effective_day =
        entry_dict.FindKey(kWindowLimitEntryEffectiveDay);
    const base::Value* starts_at =
        entry_dict.FindKey(kWindowLimitEntryStartsAt);
    const base::Value* ends_at = entry_dict.FindKey(kWindowLimitEntryEndsAt);
    const base::Value* last_updated_value =
        entry_dict.FindKey(kTimeLimitLastUpdatedAt);

    if (!effective_day || !starts_at || !ends_at || !last_updated_value) {
      // Missing information, so this entry will be ignored.
      continue;
    }

    int64_t last_updated;
    if (!base::StringToInt64(last_updated_value->GetString(), &last_updated)) {
      // Cannot process entry without a valid last updated.
      continue;
    }

    TimeWindowLimitEntry entry;
    entry.starts_at = ValueToTimeDelta(starts_at);
    entry.ends_at = ValueToTimeDelta(ends_at);
    entry.last_updated = base::Time::UnixEpoch() +
                         base::TimeDelta::FromMilliseconds(last_updated);

    Weekday weekday = GetWeekday(effective_day->GetString());
    // We only support one time_limit_window per day. If more than one is sent
    // we only use the latest updated.
    if (!entries[weekday] ||
        entries[weekday]->last_updated < entry.last_updated) {
      entries[weekday] = std::move(entry);
    }
  }
}

TimeWindowLimit::~TimeWindowLimit() = default;

TimeWindowLimit::TimeWindowLimit(TimeWindowLimit&&) = default;

TimeWindowLimit& TimeWindowLimit::operator=(TimeWindowLimit&&) = default;

TimeUsageLimitEntry::TimeUsageLimitEntry() = default;

TimeUsageLimit::TimeUsageLimit(const base::Value& usage_limit_dict)
    // Default reset time is midnight.
    : resets_at(base::TimeDelta::FromMinutes(0)) {
  const base::Value* resets_at_value =
      usage_limit_dict.FindKey(kUsageLimitResetAt);
  if (resets_at_value) {
    resets_at = ValueToTimeDelta(resets_at_value);
  }

  for (const std::string& weekday_key : kTimeLimitWeekdays) {
    if (!usage_limit_dict.FindKey(weekday_key))
      continue;

    const base::Value* entry_dict = usage_limit_dict.FindKey(weekday_key);

    const base::Value* usage_quota = entry_dict->FindKey(kUsageLimitUsageQuota);
    const base::Value* last_updated_value =
        entry_dict->FindKey(kTimeLimitLastUpdatedAt);

    int64_t last_updated;
    if (!base::StringToInt64(last_updated_value->GetString(), &last_updated)) {
      // Cannot process entry without a valid last updated.
      continue;
    }

    Weekday weekday = GetWeekday(weekday_key);
    TimeUsageLimitEntry entry;
    entry.usage_quota = base::TimeDelta::FromMinutes(usage_quota->GetInt());
    entry.last_updated = base::Time::UnixEpoch() +
                         base::TimeDelta::FromMilliseconds(last_updated);
    entries[weekday] = std::move(entry);
  }
}

TimeUsageLimit::~TimeUsageLimit() = default;

TimeUsageLimit::TimeUsageLimit(TimeUsageLimit&&) = default;

TimeUsageLimit& TimeUsageLimit::operator=(TimeUsageLimit&&) = default;

Override::Override(const base::Value& override_dict) {
  const base::Value* action_value = override_dict.FindKey(kOverrideAction);
  const base::Value* created_at_value =
      override_dict.FindKey(kOverrideActionCreatedAt);

  if (!action_value || !created_at_value)
    return;

  int64_t created_at_millis;
  if (!base::StringToInt64(created_at_value->GetString(), &created_at_millis)) {
    // Cannot process entry without a valid creation time.
    return;
  }

  action = action_value->GetString() == kOverrideActionLock ? Action::kLock
                                                            : Action::kUnlock;
  created_at = base::Time::UnixEpoch() +
               base::TimeDelta::FromMilliseconds(created_at_millis);

  const base::Value* duration_value = override_dict.FindPath(
      {kOverrideActionSpecificData, kOverrideActionDurationMins});
  if (duration_value)
    duration = base::TimeDelta::FromMinutes(duration_value->GetInt());
}

Override::~Override() = default;

Override::Override(Override&&) = default;

Override& Override::operator=(Override&&) = default;

Weekday GetWeekday(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return static_cast<Weekday>(exploded.day_of_week);
}

Weekday WeekdayShift(Weekday current_day, int shift) {
  return static_cast<Weekday>((static_cast<int>(current_day) + shift) %
                              static_cast<int>(Weekday::kCount));
}

}  // namespace internal

base::Optional<internal::TimeWindowLimit> TimeWindowLimitFromPolicy(
    const std::unique_ptr<base::DictionaryValue>& time_limit) {
  base::Value* time_window_limit_value =
      time_limit->FindKey(internal::kTimeWindowLimit);
  if (!time_window_limit_value)
    return base::nullopt;
  return internal::TimeWindowLimit(*time_window_limit_value);
}

base::Optional<internal::TimeUsageLimit> TimeUsageLimitFromPolicy(
    const std::unique_ptr<base::DictionaryValue>& time_limit) {
  base::Value* time_usage_limit_value =
      time_limit->FindKey(internal::kTimeUsageLimit);
  if (!time_usage_limit_value)
    return base::nullopt;
  return internal::TimeUsageLimit(*time_usage_limit_value);
}

base::Optional<internal::Override> OverrideFromPolicy(
    const std::unique_ptr<base::DictionaryValue>& time_limit) {
  base::Value* override_value = time_limit->FindKey(internal::kOverride);
  if (!override_value)
    return base::nullopt;
  return internal::Override(*override_value);
}

State GetState(const std::unique_ptr<base::DictionaryValue>& time_limit,
               const base::TimeDelta& used_time,
               const base::Time& usage_timestamp,
               const base::Time& current_time,
               const base::Optional<State>& previous_state) {
  base::Optional<internal::TimeWindowLimit> time_window_limit =
      TimeWindowLimitFromPolicy(time_limit);
  base::Optional<internal::TimeUsageLimit> time_usage_limit =
      TimeUsageLimitFromPolicy(time_limit);
  base::Optional<internal::Override> override = OverrideFromPolicy(time_limit);
  return internal::UsageTimeLimitProcessor(
             std::move(time_window_limit), std::move(time_usage_limit),
             std::move(override), used_time, current_time, current_time,
             previous_state)
      .GetState();
}

base::Time GetExpectedResetTime(
    const std::unique_ptr<base::DictionaryValue>& time_limit,
    const base::Time current_time) {
  base::Optional<internal::TimeWindowLimit> time_window_limit =
      TimeWindowLimitFromPolicy(time_limit);
  base::Optional<internal::TimeUsageLimit> time_usage_limit =
      TimeUsageLimitFromPolicy(time_limit);
  base::Optional<internal::Override> override = OverrideFromPolicy(time_limit);
  return internal::UsageTimeLimitProcessor(
             std::move(time_window_limit), std::move(time_usage_limit),
             std::move(override), base::TimeDelta::FromMinutes(0), base::Time(),
             current_time, base::nullopt)
      .GetExpectedResetTime();
}

}  // namespace usage_time_limit
}  // namespace chromeos
