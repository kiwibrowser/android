// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder_impl.h"

#include <memory>

#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

namespace {
const base::Time kTestTime = base::Time::FromJavaTime(1500000000000);
}  // namespace

class SetupFlowCompletionRecorderImplTest : public testing::Test {
 protected:
  SetupFlowCompletionRecorderImplTest() = default;
  ~SetupFlowCompletionRecorderImplTest() override = default;

  void SetUp() override {
    test_pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    SetupFlowCompletionRecorderImpl::RegisterPrefs(
        test_pref_service_->registry());
    test_clock_ = std::make_unique<base::SimpleTestClock>();
    test_clock_->SetNow(kTestTime);

    recorder_ = SetupFlowCompletionRecorderImpl::Factory::Get()->BuildInstance(
        test_pref_service_.get(), test_clock_.get());
  }

  SetupFlowCompletionRecorder* recorder() { return recorder_.get(); }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<base::SimpleTestClock> test_clock_;

  std::unique_ptr<SetupFlowCompletionRecorder> recorder_;

  DISALLOW_COPY_AND_ASSIGN(SetupFlowCompletionRecorderImplTest);
};

TEST_F(SetupFlowCompletionRecorderImplTest, RecordsCorrectTime) {
  EXPECT_FALSE(recorder()->GetCompletionTimestamp());
  recorder()->RecordCompletion();
  EXPECT_EQ(kTestTime, recorder()->GetCompletionTimestamp());
  EXPECT_EQ(kTestTime, recorder()->GetCompletionTimestamp());
}
}  // namespace multidevice_setup

}  // namespace chromeos
