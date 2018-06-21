// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_autoupdate_time_restrictions_proto_parser.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/weekly_time/weekly_time.h"
#include "chrome/browser/chromeos/policy/weekly_time/weekly_time_interval.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {
constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[]{
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

}  // namespace

TEST(DeviceAutoUpdateTimeRestrictionsErrorTest, EmptyProto) {
  em::AutoUpdateSettingsProto proto;
  auto result = AutoUpdateDisallowedTimeIntervalsToValue(proto);
  EXPECT_FALSE(result);
}

TEST(DeviceAutoUpdateTimeRestrictionsErrorTest, AllInvalidIntervals) {
  em::AutoUpdateSettingsProto settings_proto;
  em::WeeklyTimeIntervalProto interval_proto;
  // Proto with invalid fields.
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[0]);
  start->set_time(1000);
  end->set_day_of_week(kWeekdays[3]);
  end->set_time(1000);
  *settings_proto.add_disallowed_time_intervals() = interval_proto;
  // Empty proto.
  settings_proto.add_disallowed_time_intervals();
  auto result = AutoUpdateDisallowedTimeIntervalsToValue(settings_proto);
  EXPECT_FALSE(result);
}

// Represents a WeeklyInterval as a tuple
using TestInterval = std::tuple<bool /* is_valid_interval */,
                                int /* start_day_of_week */,
                                int /* start_time */,
                                int /* end_day_of_week */,
                                int /* end_time */>;

class DeviceAutoUpdateTimeRestrictionsTest
    : public testing::TestWithParam<std::vector<TestInterval>> {
 protected:
  ParamType intervals() const { return GetParam(); }
  em::WeeklyTimeIntervalProto ConvertTupleToWeeklyIntervalProto(
      const TestInterval& time_tuple) {
    em::WeeklyTimeIntervalProto interval_proto;
    em::WeeklyTimeProto* start = interval_proto.mutable_start();
    em::WeeklyTimeProto* end = interval_proto.mutable_end();
    start->set_day_of_week(kWeekdays[std::get<1>(time_tuple)]);
    start->set_time(std::get<2>(time_tuple));
    end->set_day_of_week(kWeekdays[std::get<3>(time_tuple)]);
    end->set_time(std::get<4>(time_tuple));
    return interval_proto;
  }

  em::AutoUpdateSettingsProto MakeAutoUpdateSettingsProto(
      const ParamType& intervals) {
    em::AutoUpdateSettingsProto proto;
    for (const auto& interval : intervals) {
      *proto.add_disallowed_time_intervals() =
          ConvertTupleToWeeklyIntervalProto(interval);
    }
    return proto;
  }

  std::unique_ptr<WeeklyTimeInterval> TupleToWeeklyTimeInterval(
      const TestInterval& time_tuple) {
    bool is_valid_interval = std::get<0>(time_tuple);
    if (!is_valid_interval) {
      return nullptr;
    }
    int start_day = std::get<1>(time_tuple);
    int start_time = std::get<2>(time_tuple);
    int end_day = std::get<3>(time_tuple);
    int end_time = std::get<4>(time_tuple);
    return std::make_unique<WeeklyTimeInterval>(
        WeeklyTime(start_day, start_time), WeeklyTime(end_day, end_time));
  }
};

TEST_P(DeviceAutoUpdateTimeRestrictionsTest, ValidTest) {
  em::AutoUpdateSettingsProto proto = MakeAutoUpdateSettingsProto(intervals());
  auto result = AutoUpdateDisallowedTimeIntervalsToValue(proto);
  base::ListValue expected;
  for (const auto& tuple : intervals()) {
    auto interval = TupleToWeeklyTimeInterval(tuple);
    if (interval) {
      expected.Append(interval->ToValue());
    }
  }
  EXPECT_TRUE(result);
  EXPECT_EQ(*result, expected);
}

INSTANTIATE_TEST_CASE_P(OneInterval,
                        DeviceAutoUpdateTimeRestrictionsTest,
                        testing::Values(std::vector<TestInterval>{
                            std::make_tuple(true, 1, 1500, 2, 1000)}));

INSTANTIATE_TEST_CASE_P(MultipleIntervals,
                        DeviceAutoUpdateTimeRestrictionsTest,
                        testing::Values(std::vector<TestInterval>{
                            std::make_tuple(true, 1, 1500, 2, 1000),
                            std::make_tuple(true, 1, 15, 5, 10),
                            std::make_tuple(true, 1, 200, 6, 100),
                            std::make_tuple(true, 4, 10, 3, 1)}));

INSTANTIATE_TEST_CASE_P(MixedGoodAndBadIntervals,
                        DeviceAutoUpdateTimeRestrictionsTest,
                        testing::Values(std::vector<TestInterval>{
                            std::make_tuple(true, 1, 1500, 2, 1000),
                            std::make_tuple(false, 0, 15, 5, 10),
                            std::make_tuple(true, 1, 200, 6, 100),
                            std::make_tuple(false, 4, 10, 4, -100)}));

}  // namespace policy
