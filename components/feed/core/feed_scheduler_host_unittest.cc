// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include "base/bind.h"
#include "base/test/simple_test_clock.h"
#include "components/feed/core/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

// Fixed "now" to make tests more deterministic.
char kNowString[] = "2018-06-11 15:41";

using base::Time;

class FeedSchedulerHostTest : public ::testing::Test {
 public:
  void TriggerRefresh() { trigger_refresh_count_++; }

 protected:
  FeedSchedulerHostTest() : scheduler_(&pref_service_, &test_clock_) {
    Time now;
    CHECK(Time::FromUTCString(kNowString, &now));
    test_clock_.SetNow(now);

    FeedSchedulerHost::RegisterProfilePrefs(pref_service_.registry());
  }

  PrefService* pref_service() { return &pref_service_; }
  base::SimpleTestClock* test_clock() { return &test_clock_; }
  FeedSchedulerHost* scheduler() { return &scheduler_; }
  int trigger_refresh_count() { return trigger_refresh_count_; }

 private:
  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock test_clock_;
  FeedSchedulerHost scheduler_;
  int trigger_refresh_count_ = 0;
};

TEST_F(FeedSchedulerHostTest, ShouldSessionRequestDataSimple) {
  EXPECT_EQ(REQUEST_WITH_WAIT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
  // TODO(skym): REQUEST_WITH_TIMEOUT.
  EXPECT_EQ(REQUEST_WITH_CONTENT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ false));
  EXPECT_EQ(NO_REQUEST_WITH_WAIT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ false, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));
  // TODO(skym): NO_REQUEST_WITH_TIMEOUT.
  EXPECT_EQ(NO_REQUEST_WITH_CONTENT,
            scheduler()->ShouldSessionRequestData(
                /*has_content*/ true, /*content_creation_date_time*/ Time(),
                /*has_outstanding_request*/ true));
}

TEST_F(FeedSchedulerHostTest, OnReceiveNewContentVerifyPref) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnReceiveNewContent(Time());
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnRequestErrorVerifyPref) {
  EXPECT_EQ(Time(), pref_service()->GetTime(prefs::kLastFetchAttemptTime));
  scheduler()->OnRequestError(0);
  EXPECT_EQ(test_clock()->Now(),
            pref_service()->GetTime(prefs::kLastFetchAttemptTime));
}

TEST_F(FeedSchedulerHostTest, OnForegroundedTriggersRefresh) {
  scheduler()->RegisterTriggerRefreshCallback(base::BindRepeating(
      &FeedSchedulerHostTest::TriggerRefresh, base::Unretained(this)));
  scheduler()->OnForegrounded();
  EXPECT_EQ(1, trigger_refresh_count());
}

TEST_F(FeedSchedulerHostTest, OnFixedTimerTriggersRefresh) {
  scheduler()->RegisterTriggerRefreshCallback(base::BindRepeating(
      &FeedSchedulerHostTest::TriggerRefresh, base::Unretained(this)));
  scheduler()->OnFixedTimer();
  EXPECT_EQ(1, trigger_refresh_count());
}

}  // namespace

}  // namespace feed
