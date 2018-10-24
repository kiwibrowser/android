// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/ukm_time_aggregator.h"

#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"

namespace blink {
namespace {

// These tests must use metrics defined in ukm.xml
const char* kEvent = "Blink.UpdateTime";
const char* kMetric1 = "Compositing";
const char* kMetric1Average = "Compositing.Average";
const char* kMetric1WorstCase = "Compositing.WorstCase";
const char* kMetric2 = "Paint";
const char* kMetric2Average = "Paint.Average";
const char* kMetric2WorstCase = "Paint.WorstCase";

TEST(UkmTimeAggregatorTest, EmptyEventsNotRecorded) {
  WTF::ScopedMockClock clock;
  ukm::TestUkmRecorder recorder;
  int64_t source_id = ukm::UkmRecorder::GetNewSourceID();
  std::unique_ptr<UkmTimeAggregator> aggregator(
      new UkmTimeAggregator(kEvent, source_id, &recorder, {kMetric1, kMetric2},
                            TimeDelta::FromSeconds(1)));
  clock.Advance(TimeDelta::FromSeconds(10));
  aggregator.reset();

  EXPECT_EQ(recorder.sources_count(), 0u);
  EXPECT_EQ(recorder.entries_count(), 0u);
}

TEST(UkmTimeAggregatorTest, EventsRecordedPerSecond) {
  WTF::ScopedMockClock clock;
  ukm::TestUkmRecorder recorder;
  int64_t source_id = ukm::UkmRecorder::GetNewSourceID();
  std::unique_ptr<UkmTimeAggregator> aggregator(
      new UkmTimeAggregator(kEvent, source_id, &recorder, {kMetric1, kMetric2},
                            TimeDelta::FromSeconds(1)));
  // Have 100 events 99ms each; if the records are recorded once per second, we
  // should expect 9 records to be recorded while the timer ticks. 0-1, 1-2,
  // ..., 8-9 seconds.
  for (int i = 0; i < 100; ++i) {
    auto timer = aggregator->GetScopedTimer(i % 2);
    clock.Advance(TimeDelta::FromMilliseconds(99));
  }

  EXPECT_EQ(recorder.entries_count(), 9u);

  // Once we reset, we record any remaining samples into one more entry, for a
  // total of 10.
  aggregator.reset();

  EXPECT_EQ(recorder.entries_count(), 10u);
  auto entries = recorder.GetEntriesByName(kEvent);
  EXPECT_EQ(entries.size(), 10u);

  for (auto* entry : entries) {
    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric1Average));
    const int64_t* metric1_average =
        ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric1Average);
    EXPECT_NEAR(*metric1_average / 1e6, 0.099, 0.0001);

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric1WorstCase));
    const int64_t* metric1_worst =
        ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric1WorstCase);
    EXPECT_NEAR(*metric1_worst / 1e6, 0.099, 0.0001);

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric2Average));
    const int64_t* metric2_average =
        ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric2Average);
    EXPECT_NEAR(*metric2_average / 1e6, 0.099, 0.0001);

    EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric2WorstCase));
    const int64_t* metric2_worst =
        ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric2WorstCase);
    EXPECT_NEAR(*metric2_worst / 1e6, 0.099, 0.0001);
  }
}

TEST(UkmTimeAggregatorTest, EventsAveragedCorrectly) {
  WTF::ScopedMockClock clock;
  ukm::TestUkmRecorder recorder;
  int64_t source_id = ukm::UkmRecorder::GetNewSourceID();
  std::unique_ptr<UkmTimeAggregator> aggregator(
      new UkmTimeAggregator(kEvent, source_id, &recorder, {kMetric1, kMetric2},
                            TimeDelta::FromSeconds(10000)));
  // 1, 2, and 3 seconds.
  for (int i = 1; i <= 3; ++i) {
    auto timer = aggregator->GetScopedTimer(0);
    clock.Advance(TimeDelta::FromSeconds(i));
  }

  // 3, 3, 3, and then 1 outside of the loop.
  for (int i = 0; i < 3; ++i) {
    auto timer = aggregator->GetScopedTimer(1);
    clock.Advance(TimeDelta::FromSeconds(3));
  }
  {
    auto timer = aggregator->GetScopedTimer(1);
    clock.Advance(TimeDelta::FromSeconds(1));
  }

  aggregator.reset();
  auto entries = recorder.GetEntriesByName(kEvent);
  EXPECT_EQ(entries.size(), 1u);
  auto* entry = entries[0];

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric1Average));
  const int64_t* metric1_average =
      ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric1Average);
  // metric1 (1, 2, 3) average is 2
  EXPECT_NEAR(*metric1_average / 1e6, 2.0, 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric1WorstCase));
  const int64_t* metric1_worst =
      ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric1WorstCase);
  // metric1 (1, 2, 3) worst case is 3
  EXPECT_NEAR(*metric1_worst / 1e6, 3.0, 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric2Average));
  const int64_t* metric2_average =
      ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric2Average);
  // metric1 (3, 3, 3, 1) average is 2.5
  EXPECT_NEAR(*metric2_average / 1e6, 2.5, 0.0001);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, kMetric2WorstCase));
  const int64_t* metric2_worst =
      ukm::TestUkmRecorder::GetEntryMetric(entry, kMetric2WorstCase);
  // metric1 (3, 3, 3, 1) worst case is 3
  EXPECT_NEAR(*metric2_worst / 1e6, 3.0, 0.0001);
}

}  // namespace
}  // namespace blink
