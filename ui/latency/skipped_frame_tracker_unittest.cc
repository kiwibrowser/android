// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/latency/skipped_frame_tracker.h"

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

// TestClient observes calls to AddFrameProduced so tests can verify
// those calls.
class TestClient : public SkippedFrameTracker::Client {
 public:
  TestClient() = default;
  ~TestClient() override = default;

  void AddFrameProduced(base::TimeTicks source_timestamp,
                        base::TimeDelta amount_produced,
                        base::TimeDelta amount_skipped) override {
    source_timestamp_ = source_timestamp.since_origin().InMicroseconds();
    amount_produced_ = amount_produced.InMicroseconds();
    amount_skipped_ = amount_skipped.InMicroseconds();
    call_count_++;
  }

  int GetAndResetCallCount() {
    int result = call_count_;
    call_count_ = 0;
    return result;
  }

  int call_count_ = 0;
  int source_timestamp_ = 0;
  int amount_produced_ = 0;
  int amount_skipped_ = 0;
};

// TestSkippedFrameTracker let's us verify the active state from tests.
class TestSkippedFrameTracker : public SkippedFrameTracker {
 public:
  TestSkippedFrameTracker(Client* client) : SkippedFrameTracker(client) {}

  bool IsActive() {
    switch (active_state_) {
      case ActiveState::Idle:
        return false;
      case ActiveState::WillProduce:
      case ActiveState::WillProduceFirst:
      case ActiveState::WasActive:
        break;
    }
    return true;
  }
};

// SkippedFrameTrackerTest is the test fixture used by all tests in this file.
class SkippedFrameTrackerTest : public testing::Test {
 public:
  SkippedFrameTrackerTest() : tracker_(&client_) {}

  ::testing::AssertionResult BeginFrame(int timestamp, int interval) {
    int call_count = client_.call_count_;
    tracker_.BeginFrame(
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(timestamp),
        base::TimeDelta::FromMicroseconds(interval));
    return MaybeCallCountFailure(call_count);
  }

  ::testing::AssertionResult FinishFrame() {
    int call_count = client_.call_count_;
    tracker_.FinishFrame();
    return MaybeCallCountFailure(call_count);
  }

  ::testing::AssertionResult WillProduceFrame() {
    int call_count = client_.call_count_;
    tracker_.WillProduceFrame();
    return MaybeCallCountFailure(call_count);
  }

  ::testing::AssertionResult DidProduceFrame() {
    int call_count = client_.call_count_;
    tracker_.DidProduceFrame();
    return MaybeCallCountFailure(call_count);
  }

 protected:
  static ::testing::AssertionResult MaybeCallCountFailure(int count) {
    if (count == 0)
      return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << count << " unverified calls to AddFrameProduced.";
  }

  TestClient client_;
  TestSkippedFrameTracker tracker_;
};

#define VERIFY_ADD_PRODUCED_CALLED(timestamp, produced, skipped) \
  EXPECT_EQ(1, client_.GetAndResetCallCount());                  \
  EXPECT_EQ(timestamp, client_.source_timestamp_);               \
  EXPECT_EQ(produced, client_.amount_produced_);                 \
  EXPECT_EQ(skipped, client_.amount_skipped_);

// Producing a frame entirely within a BeginFrame works.
TEST_F(SkippedFrameTrackerTest, NoSkips_BeginThenWill) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
}

// Starting to produce a frame before receiving the BeginFrame works.
TEST_F(SkippedFrameTrackerTest, NoSkips_WillThenBegin) {
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
}

// A (WillProduceFrame, DidProduceFrame) that spans multiple BeginFrames
// is registered properly.
TEST_F(SkippedFrameTrackerTest, Skips_ProducedOverMultipleBeginFrames) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(110, 10, 10);
  EXPECT_TRUE(FinishFrame());
}

// An unexpected jump in the frame timestamp, compared to the interval,
// is registered as skipped time.
TEST_F(SkippedFrameTrackerTest, Skips_DroppedBeginFrames) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());

  EXPECT_TRUE(BeginFrame(200, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(200, 10, 90);
  EXPECT_TRUE(FinishFrame());
}

// Jitter just below the interval midpoint rounds down the number of dropped
// BeginFrames detected.
TEST_F(SkippedFrameTrackerTest, Skips_DroppedBeginFrames_JitterRoundsDown) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(BeginFrame(114, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(114, 10, 10);
  EXPECT_TRUE(FinishFrame());
}

// Jitter just above the interval midpoint rounds up the number of dropped
// BeginFrames detected.
TEST_F(SkippedFrameTrackerTest, Skips_DroppedBeginFrames_JitterRoundsUp) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(BeginFrame(116, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(116, 10, 20);
  EXPECT_TRUE(FinishFrame());
}

// Active, idle, then active again.
// In second active period, start to produce frame first.
TEST_F(SkippedFrameTrackerTest, NoSkips_ActiveIdleActive_WillThenBegin) {
  // Active
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());

  // Idle
  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(FinishFrame());

  // Active
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(BeginFrame(120, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(120, 10, 0);
  EXPECT_TRUE(FinishFrame());
}

// Active, idle, then active again.
// In second active period, BeginFrame first.
TEST_F(SkippedFrameTrackerTest, NoSkips_ActiveIdleActive_BeginThenWill) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());

  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(FinishFrame());
  EXPECT_FALSE(tracker_.IsActive());

  EXPECT_TRUE(BeginFrame(120, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(120, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());
}

// Active, idle, then active again.
// Dropped BeginFrames during idle period shouldn't register as skipped.
TEST_F(SkippedFrameTrackerTest, NoSkips_ActiveIdleActive_JumpInIdle) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());

  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(FinishFrame());
  EXPECT_FALSE(tracker_.IsActive());

  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(BeginFrame(200, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(200, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());
}

// If frames are pulled from later in the pipeline when the source hasn't tried
// to create a new frame, it should not be recorded as a frame produced
// by the source.
TEST_F(SkippedFrameTrackerTest, PulledFramesNotRecorded) {
  EXPECT_TRUE(BeginFrame(100, 10));
  // WillProduceFrame intentionally not called here impliles
  // next call to DidProduceFrame was "pulled" not "pushed".
  EXPECT_TRUE(DidProduceFrame());
  EXPECT_TRUE(FinishFrame());

  // Even though BeginFrames might've been dropped since the pulled frame,
  // act as if we should behanve just like the produce is coming out of an
  // idle period.
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(BeginFrame(200, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(200, 10, 0);
  EXPECT_TRUE(FinishFrame());
}

// Multiple calls to WillProduceFrame are legal and should behave as if only
// the first call was made.
TEST_F(SkippedFrameTrackerTest, MultipleWillProduceBeforeDidProduce) {
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
}

// Frame pulled before BeginFrame doesn't count.
TEST_F(SkippedFrameTrackerTest, NoSkips_ActiveIdleActive_FramePulledBeforeBF) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());

  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(FinishFrame());
  EXPECT_FALSE(tracker_.IsActive());

  EXPECT_TRUE(WillProduceFrame());
  // Consider frame pulled since it came before the BeginFrame.
  EXPECT_TRUE(DidProduceFrame());
  // Make sure we are immune to multiple pulled frames.
  EXPECT_TRUE(DidProduceFrame());

  EXPECT_TRUE(BeginFrame(120, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(120, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());
}

// Frame pulled just after a push doesn't count.
TEST_F(SkippedFrameTrackerTest, NoSkips_ActiveIdleActive_FramePulledAfterPush) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());

  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(FinishFrame());
  EXPECT_FALSE(tracker_.IsActive());

  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(BeginFrame(120, 10));
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(120, 10, 0);
  // Consider frame pulled since we aleady pushed one this frame.
  EXPECT_TRUE(DidProduceFrame());
  // Make sure we are immune to multiple pulled frames.
  EXPECT_TRUE(DidProduceFrame());
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());
}

// Frame pulled while attempting to push counts.
TEST_F(SkippedFrameTrackerTest, NoSkips_ActiveIdleActive_FramePulledIsPush) {
  EXPECT_TRUE(BeginFrame(100, 10));
  EXPECT_TRUE(WillProduceFrame());
  EXPECT_TRUE(FinishFrame());
  EXPECT_TRUE(tracker_.IsActive());

  // Consider frame pushed, even if we are outside the BeginFrame, since we
  // were trying to push.
  EXPECT_TRUE(DidProduceFrame());
  VERIFY_ADD_PRODUCED_CALLED(100, 10, 0);
  // A second pulled frame shouldn't count though.
  EXPECT_TRUE(DidProduceFrame());

  EXPECT_TRUE(BeginFrame(110, 10));
  EXPECT_TRUE(FinishFrame());
  EXPECT_FALSE(tracker_.IsActive());
}

}  // namespace
}  // namespace ui
