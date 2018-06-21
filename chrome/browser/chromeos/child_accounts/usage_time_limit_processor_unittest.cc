// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "usage_time_limit_processor.h"

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace usage_time_limit {

using UsageTimeLimitProcessorTest = testing::Test;

base::Value CreateTime(int hour, int minute) {
  base::Value time(base::Value::Type::DICTIONARY);
  time.SetKey("hour", base::Value(hour));
  time.SetKey("minute", base::Value(minute));
  return time;
}

base::Value CreateTimeWindow(base::Value day,
                             base::Value start,
                             base::Value end,
                             base::Value last_updated) {
  base::Value time_window(base::Value::Type::DICTIONARY);
  time_window.SetKey("effective_day", std::move(day));
  time_window.SetKey("starts_at", std::move(start));
  time_window.SetKey("ends_at", std::move(end));
  time_window.SetKey("last_updated_millis", std::move(last_updated));
  return time_window;
}

base::Value CreateTimeUsage(base::Value usage_quota, base::Value last_updated) {
  base::Value time_usage(base::Value::Type::DICTIONARY);
  time_usage.SetKey("usage_quota_mins", std::move(usage_quota));
  time_usage.SetKey("last_updated_millis", std::move(last_updated));
  return time_usage;
}

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  if (!base::Time::FromUTCString(time_string, &time)) {
    LOG(ERROR) << "Wrong time string format.";
  }
  return time;
}

std::string CreatePolicyTimestamp(const char* time_string) {
  base::Time time = TimeFromString(time_string);
  return std::to_string(
      base::TimeDelta(time - base::Time::UnixEpoch()).InMilliseconds());
}

void AssertEqState(State expected, State actual) {
  ASSERT_EQ(expected.is_locked, actual.is_locked);
  ASSERT_EQ(expected.active_policy, actual.active_policy);
  ASSERT_EQ(expected.is_time_usage_limit_enabled,
            actual.is_time_usage_limit_enabled);

  if (actual.is_time_usage_limit_enabled) {
    ASSERT_EQ(expected.remaining_usage, actual.remaining_usage);
    if (actual.remaining_usage <= base::TimeDelta::FromMinutes(0)) {
      ASSERT_EQ(expected.time_usage_limit_started,
                actual.time_usage_limit_started);
    }
  }

  ASSERT_EQ(expected.next_state_change_time, actual.next_state_change_time);
  ASSERT_EQ(expected.next_state_active_policy, actual.next_state_active_policy);
  ASSERT_EQ(expected.last_state_changed, actual.last_state_changed);
}

namespace internal {

using UsageTimeLimitProcessorInternalTest = testing::Test;

TEST_F(UsageTimeLimitProcessorInternalTest, TimeLimitWindowValid) {
  std::string last_updated_millis =
      CreatePolicyTimestamp("1 Jan 1970 00:00:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(22, 30),
                       CreateTime(7, 30), base::Value(last_updated_millis));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(23, 0),
                       CreateTime(8, 20), base::Value(last_updated_millis));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit = base::Value(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Call tested function.
  TimeWindowLimit window_limit_struct(time_window_limit);

  ASSERT_TRUE(window_limit_struct.entries[Weekday::kMonday]);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kMonday]
                .value()
                .starts_at.InMinutes(),
            22 * 60 + 30);
  ASSERT_EQ(
      window_limit_struct.entries[Weekday::kMonday].value().ends_at.InMinutes(),
      7 * 60 + 30);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kMonday].value().last_updated,
            base::Time::UnixEpoch());

  ASSERT_TRUE(window_limit_struct.entries[Weekday::kFriday]);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kFriday]
                .value()
                .starts_at.InMinutes(),
            23 * 60);
  ASSERT_EQ(
      window_limit_struct.entries[Weekday::kFriday].value().ends_at.InMinutes(),
      8 * 60 + 20);
  ASSERT_EQ(window_limit_struct.entries[Weekday::kFriday].value().last_updated,
            base::Time::UnixEpoch());

  // Assert that weekdays without time_window_limits are not set.
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kTuesday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kWednesday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kThursday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kSaturday]);
  ASSERT_FALSE(window_limit_struct.entries[Weekday::kSunday]);
}

// Validates that a well formed dictionary containing the time_usage_limit
// information from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorInternalTest, TimeUsageWindowValid) {
  // Create dictionary containing the policy information.
  std::string last_updated_millis_one =
      CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  std::string last_updated_millis_two =
      CreatePolicyTimestamp("1 Jan 2018 11:00:00");
  base::Value tuesday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated_millis_one));
  base::Value thursday_time_usage =
      CreateTimeUsage(base::Value(80), base::Value(last_updated_millis_two));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("tuesday", std::move(tuesday_time_usage));
  time_usage_limit.SetKey("thursday", std::move(thursday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Call tested functions.
  TimeUsageLimit usage_limit_struct(time_usage_limit);

  ASSERT_EQ(usage_limit_struct.resets_at.InMinutes(), 8 * 60);

  ASSERT_EQ(usage_limit_struct.entries[Weekday::kTuesday]
                .value()
                .usage_quota.InMinutes(),
            120);
  ASSERT_EQ(usage_limit_struct.entries[Weekday::kTuesday].value().last_updated,
            base::Time::FromDoubleT(1514800800));

  ASSERT_EQ(usage_limit_struct.entries[Weekday::kThursday]
                .value()
                .usage_quota.InMinutes(),
            80);
  ASSERT_EQ(usage_limit_struct.entries[Weekday::kThursday].value().last_updated,
            base::Time::FromDoubleT(1514804400));

  // Assert that weekdays without time_usage_limits are not set.
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kMonday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kWednesday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kFriday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kSaturday]);
  ASSERT_FALSE(usage_limit_struct.entries[Weekday::kSunday]);
}

// Validates that a well formed dictionary containing the override information
// from the UsageTimeLimit policy is converted to its intermediate
// representation correctly.
TEST_F(UsageTimeLimitProcessorInternalTest, OverrideValid) {
  // Create policy information.
  std::string created_at_millis = CreatePolicyTimestamp("1 Jan 2018 10:00:00");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("UNLOCK"));
  override.SetKey("created_at_millis", base::Value(created_at_millis));

  // Call tested functions.
  Override override_struct(override);

  // Assert right fields are set.
  ASSERT_EQ(override_struct.action, Override::Action::kUnlock);
  ASSERT_EQ(override_struct.created_at, base::Time::FromDoubleT(1514800800));
  ASSERT_FALSE(override_struct.duration);
}

}  // namespace internal

// Tests the GetState for a policy that only have the time window limit set. It
// is checked that the state is correct before, during and after the policy is
// enforced.
TEST_F(UsageTimeLimitProcessorTest, GetStateOnlyTimeWindowLimitSet) {
  // Set up policy.
  std::string last_updated_millis = CreatePolicyTimestamp("1 Jan 2018 10:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(7, 30), base::Value(last_updated_millis));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(21, 0),
                       CreateTime(7, 30), base::Value(last_updated_millis));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  base::Time monday_time_window_limit_start =
      TimeFromString("Mon, 1 Jan 2018 21:00");
  base::Time monday_time_window_limit_end =
      TimeFromString("Tue, 2 Jan 2018 7:30");
  base::Time friday_time_window_limit_start =
      TimeFromString("Fri, 5 Jan 2018 21:00");

  // Check state before Monday time window limit.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 20:00");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_one,
               time_one, base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time = monday_time_window_limit_start;
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check state during the Monday time window limit.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 22:00");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_two,
               time_two, state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time = monday_time_window_limit_end;
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);

  // Check state after the Monday time window limit.
  base::Time time_three = TimeFromString("Tue, 2 Jan 2018 9:00");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0),
               time_three, time_three, state_two);

  State expected_state_three;
  expected_state_three.is_locked = false;
  expected_state_three.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_three.is_time_usage_limit_enabled = false;
  expected_state_three.next_state_change_time = friday_time_window_limit_start;
  expected_state_three.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);
}

// Tests the GetState for a policy that only have the time usage limit set. It
// is checked that the state is correct before and during the policy is
// enforced.
TEST_F(UsageTimeLimitProcessorTest, GetStateOnlyTimeUsageLimitSet) {
  // Set up policy.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00");
  base::Value tuesday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated));
  base::Value thursday_time_usage =
      CreateTimeUsage(base::Value(80), base::Value(last_updated));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("tuesday", std::move(tuesday_time_usage));
  time_usage_limit.SetKey("thursday", std::move(thursday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check state before time usage limit is enforced.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 20:00");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_one, time_one, base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = false;
  // Next state is the minimum time when the time usage limit could be enforced.
  expected_state_one.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 10:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check state before time usage limit is enforced.
  base::Time time_two = TimeFromString("Tue, 2 Jan 2018 12:00");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(60),
               time_two, time_two, state_one);

  State expected_state_two;
  expected_state_two.is_locked = false;
  expected_state_two.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(60);
  expected_state_two.next_state_change_time =
      time_two + base::TimeDelta::FromMinutes(60);
  expected_state_two.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.last_state_changed = base::Time();

  AssertEqState(expected_state_two, state_two);

  // Check state when the time usage limit should be enforced.
  base::Time time_three = TimeFromString("Tue, 2 Jan 2018 21:00");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_three, time_three, state_two);

  base::Time wednesday_reset_time = TimeFromString("Wed, 3 Jan 2018 8:00");

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kUsageLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_three;
  expected_state_three.next_state_change_time = wednesday_reset_time;
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);
}

// Test GetState with both time window limit and time usage limit defined.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithTimeUsageAndWindowLimitActive) {
  // Setup time window limit.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(8, 30), base::Value(last_updated));
  base::Value friday_time_limit =
      CreateTimeWindow(base::Value("FRIDAY"), CreateTime(21, 0),
                       CreateTime(8, 30), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));
  window_limit_entries.GetList().push_back(std::move(friday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Setup time usage limit.
  base::Value monday_time_usage =
      CreateTimeUsage(base::Value(120), base::Value(last_updated));
  base::Value thursday_time_usage =
      CreateTimeUsage(base::Value(80), base::Value(last_updated));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("monday", std::move(monday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check state before any policy is enforced.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 14:00");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(80),
               time_one, time_one, base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(40);
  expected_state_one.next_state_change_time =
      time_one + base::TimeDelta::FromMinutes(40);
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check state during time usage limit.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 16:00");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(121),
               time_two, time_two, state_one);

  base::Time monday_time_window_limit_start =
      TimeFromString("Mon, 1 Jan 2018 21:00");

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time = monday_time_window_limit_start;
  expected_state_two.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);

  // Check state during time window limit and time usage limit enforced.
  base::Time time_three = TimeFromString("Mon, 1 Jan 2018 21:00");
  State state_three =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_three, time_three, state_two);

  State expected_state_three;
  expected_state_three.is_locked = true;
  expected_state_three.active_policy = ActivePolicies::kFixedLimit;
  expected_state_three.is_time_usage_limit_enabled = true;
  expected_state_three.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_three.time_usage_limit_started = time_two;
  expected_state_three.next_state_change_time =
      TimeFromString("e, 2 Jan 2018 8:30");
  expected_state_three.next_state_active_policy =
      ActivePolicies::kNoActivePolicy;
  expected_state_three.last_state_changed = time_three;

  AssertEqState(expected_state_three, state_three);

  // Check state after time usage limit reset and window limit end.
  base::Time time_four = TimeFromString("Fri, 5 Jan 2018 8:30");
  State state_four =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_four, time_four, state_three);

  State expected_state_four;
  expected_state_four.is_locked = false;
  expected_state_four.active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_four.is_time_usage_limit_enabled = false;
  expected_state_four.next_state_change_time =
      TimeFromString("Fri, 5 Jan 2018 21:00");
  expected_state_four.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_four.last_state_changed = time_four;

  AssertEqState(expected_state_four, state_four);
}

// Check GetState when a lock override is active.
TEST_F(UsageTimeLimitProcessorTest, GetStateWithOverrideLock) {
  std::string created_at = CreatePolicyTimestamp("1 Jan 2018 15:00");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("LOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("overrides", std::move(override));
  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 15:05");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(0), time_one,
               time_one, base::nullopt);

  // Check that the device is locked until next morning.
  State expected_state_one;
  expected_state_one.is_locked = true;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 0:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);
}

// Test GetState when a overridden time window limit has been updated, so the
// override should not be aplicable anymore.
TEST_F(UsageTimeLimitProcessorTest, GetStateUpdateUnlockedTimeWindowLimit) {
  // Setup time window limit.
  std::string last_updated = CreatePolicyTimestamp("Mon, 1 Jan 2018 8:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(18, 0),
                       CreateTime(7, 30), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Setup override.
  std::string created_at = CreatePolicyTimestamp("Mon, 1 Jan 2018 18:30");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("UNLOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));

  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("overrides", std::move(override));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that the override is invalidating the time window limit.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 18:35");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_one, time_one, base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = false;
  expected_state_one.next_state_change_time =
      TimeFromString("Mon, 8 Jan 2018 18:00");
  expected_state_one.next_state_active_policy = ActivePolicies::kFixedLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Change time window limit
  std::string last_updated_two = CreatePolicyTimestamp("Mon, 1 Jan 2018 19:00");
  base::Value monday_time_limit_two =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(18, 0),
                       CreateTime(8, 00), base::Value(last_updated_two));
  base::Value window_limit_entries_two(base::Value::Type::LIST);
  window_limit_entries_two.GetList().push_back(
      std::move(monday_time_limit_two));
  base::Value time_window_limit_two(base::Value::Type::DICTIONARY);
  time_window_limit_two.SetKey("entries", std::move(window_limit_entries_two));
  time_limit_dictionary->SetKey("time_window_limit",
                                std::move(time_window_limit_two));

  // Check that the new time window limit is enforced.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 19:10");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(120),
               time_two, time_two, state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kFixedLimit;
  expected_state_two.is_time_usage_limit_enabled = false;
  expected_state_two.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 8:00");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);
}

// Make sure that the override will only affect policies that started being
// enforced before it was created.
TEST_F(UsageTimeLimitProcessorTest, GetStateOverrideTimeWindowLimitOnly) {
  // Setup time window limit.
  std::string last_updated = CreatePolicyTimestamp("1 Jan 2018 8:00");
  base::Value monday_time_limit =
      CreateTimeWindow(base::Value("MONDAY"), CreateTime(21, 0),
                       CreateTime(10, 0), base::Value(last_updated));

  base::Value window_limit_entries(base::Value::Type::LIST);
  window_limit_entries.GetList().push_back(std::move(monday_time_limit));

  base::Value time_window_limit(base::Value::Type::DICTIONARY);
  time_window_limit.SetKey("entries", std::move(window_limit_entries));

  // Setup time usage limit.
  base::Value monday_time_usage =
      CreateTimeUsage(base::Value(60), base::Value(last_updated));

  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("monday", std::move(monday_time_usage));
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Setup override.
  std::string created_at = CreatePolicyTimestamp("Mon, 1 Jan 2018 22:00");
  base::Value override = base::Value(base::Value::Type::DICTIONARY);
  override.SetKey("action", base::Value("UNLOCK"));
  override.SetKey("created_at_millis", base::Value(created_at));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_window_limit", std::move(time_window_limit));
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  time_limit->SetKey("overrides", std::move(override));

  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that the override is unlocking the device that should be locked with
  // time window limit.
  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 22:10");
  State state_one =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(40),
               time_one, time_one, base::nullopt);

  State expected_state_one;
  expected_state_one.is_locked = false;
  expected_state_one.active_policy = ActivePolicies::kOverride;
  expected_state_one.is_time_usage_limit_enabled = true;
  expected_state_one.remaining_usage = base::TimeDelta::FromMinutes(20);
  expected_state_one.next_state_change_time =
      TimeFromString("Mon, 1 Jan 2018 22:30");
  expected_state_one.next_state_active_policy = ActivePolicies::kUsageLimit;
  expected_state_one.last_state_changed = base::Time();

  AssertEqState(expected_state_one, state_one);

  // Check that the override didn't unlock the device when the time usage limit
  // started, and that it will be locked until the time usage limit reset time,
  // and not when the time window limit ends.
  base::Time time_two = TimeFromString("Mon, 1 Jan 2018 22:30");
  State state_two =
      GetState(time_limit_dictionary, base::TimeDelta::FromMinutes(60),
               time_two, time_two, state_one);

  State expected_state_two;
  expected_state_two.is_locked = true;
  expected_state_two.active_policy = ActivePolicies::kUsageLimit;
  expected_state_two.is_time_usage_limit_enabled = true;
  expected_state_two.remaining_usage = base::TimeDelta::FromMinutes(0);
  expected_state_two.time_usage_limit_started = time_two;
  expected_state_two.next_state_change_time =
      TimeFromString("Tue, 2 Jan 2018 8:00");
  expected_state_two.next_state_active_policy = ActivePolicies::kNoActivePolicy;
  expected_state_two.last_state_changed = time_two;

  AssertEqState(expected_state_two, state_two);
}

// Test GetExpectedResetTime with an empty policy.
TEST_F(UsageTimeLimitProcessorTest, GetExpectedResetTimeWithEmptyPolicy) {
  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  base::Time time_one = TimeFromString("Mon, 1 Jan 2018 22:00");
  base::Time reset_time = GetExpectedResetTime(time_limit_dictionary, time_one);

  ASSERT_EQ(reset_time, TimeFromString("Tue, 2 Jan 2018 0:00"));
}

// Test GetExpectedResetTime with a custom time usage limit reset time.
TEST_F(UsageTimeLimitProcessorTest, GetExpectedResetTimeWithCustomPolicy) {
  // Setup time usage limit.
  base::Value time_usage_limit = base::Value(base::Value::Type::DICTIONARY);
  time_usage_limit.SetKey("reset_at", CreateTime(8, 0));

  // Setup policy.
  std::unique_ptr<base::Value> time_limit =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  time_limit->SetKey("time_usage_limit", std::move(time_usage_limit));
  std::unique_ptr<base::DictionaryValue> time_limit_dictionary =
      base::DictionaryValue::From(std::move(time_limit));

  // Check that it resets in the same day.
  base::Time time_one = TimeFromString("Tue, 2 Jan 2018 6:00");
  base::Time reset_time_one =
      GetExpectedResetTime(time_limit_dictionary, time_one);

  ASSERT_EQ(reset_time_one, TimeFromString("Tue, 2 Jan 2018 8:00"));

  // Checks that it resets on the following day.
  base::Time time_two = TimeFromString("Tue, 2 Jan 2018 10:00");
  base::Time reset_time_two =
      GetExpectedResetTime(time_limit_dictionary, time_two);

  ASSERT_EQ(reset_time_two, TimeFromString("Wed, 3 Jan 2018 8:00"));
}

}  // namespace usage_time_limit
}  // namespace chromeos
