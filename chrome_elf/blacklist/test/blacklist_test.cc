// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/strings/string16.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/install_util.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/nt_registry/nt_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BlacklistTest : public testing::Test {
 protected:
  BlacklistTest() : override_manager_() {}

  std::unique_ptr<base::win::RegKey> blacklist_registry_key_;
  registry_util::RegistryOverrideManager override_manager_;

 private:
  void SetUp() override {
    base::string16 temp;
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER, &temp));
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, temp));

    blacklist_registry_key_.reset(
        new base::win::RegKey(HKEY_CURRENT_USER,
                              install_static::GetRegistryPath()
                                  .append(blacklist::kRegistryBeaconKeyName)
                                  .c_str(),
                              KEY_QUERY_VALUE | KEY_SET_VALUE));
  }

  void TearDown() override {
    ASSERT_TRUE(nt::SetTestingOverride(nt::HKCU, base::string16()));
  }
};

//------------------------------------------------------------------------------
// Beacon tests
//------------------------------------------------------------------------------

TEST_F(BlacklistTest, Beacon) {
  // Ensure that the beacon state starts off 'running' for this version.
  LONG result = blacklist_registry_key_->WriteValue(
      blacklist::kBeaconState, blacklist::BLACKLIST_SETUP_RUNNING);
  EXPECT_EQ(ERROR_SUCCESS, result);

  result = blacklist_registry_key_->WriteValue(blacklist::kBeaconVersion,
                                               TEXT(CHROME_VERSION_STRING));
  EXPECT_EQ(ERROR_SUCCESS, result);

  // First call should find the beacon and reset it.
  EXPECT_TRUE(blacklist::ResetBeacon());

  // First call should succeed as the beacon is enabled.
  EXPECT_TRUE(blacklist::LeaveSetupBeacon());
}

void TestResetBeacon(std::unique_ptr<base::win::RegKey>& key,
                     DWORD input_state,
                     DWORD expected_output_state) {
  LONG result = key->WriteValue(blacklist::kBeaconState, input_state);
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_TRUE(blacklist::ResetBeacon());
  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  result = key->ReadValueDW(blacklist::kBeaconState, &blacklist_state);
  EXPECT_EQ(ERROR_SUCCESS, result);
  EXPECT_EQ(expected_output_state, blacklist_state);
}

TEST_F(BlacklistTest, ResetBeacon) {
  // Ensure that ResetBeacon resets properly on successful runs and not on
  // failed or disabled runs.
  TestResetBeacon(blacklist_registry_key_,
                  blacklist::BLACKLIST_SETUP_RUNNING,
                  blacklist::BLACKLIST_ENABLED);

  TestResetBeacon(blacklist_registry_key_,
                  blacklist::BLACKLIST_SETUP_FAILED,
                  blacklist::BLACKLIST_SETUP_FAILED);

  TestResetBeacon(blacklist_registry_key_,
                  blacklist::BLACKLIST_DISABLED,
                  blacklist::BLACKLIST_DISABLED);
}

TEST_F(BlacklistTest, SetupFailed) {
  // Ensure that when the number of failed tries reaches the maximum allowed,
  // the blacklist state is set to failed.
  LONG result = blacklist_registry_key_->WriteValue(
      blacklist::kBeaconState, blacklist::BLACKLIST_SETUP_RUNNING);
  EXPECT_EQ(ERROR_SUCCESS, result);

  // Set the attempt count so that on the next failure the blacklist is
  // disabled.
  result = blacklist_registry_key_->WriteValue(
      blacklist::kBeaconAttemptCount, blacklist::kBeaconMaxAttempts - 1);
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_FALSE(blacklist::LeaveSetupBeacon());

  DWORD attempt_count = 0;
  blacklist_registry_key_->ReadValueDW(blacklist::kBeaconAttemptCount,
                                       &attempt_count);
  EXPECT_EQ(attempt_count, blacklist::kBeaconMaxAttempts);

  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  result = blacklist_registry_key_->ReadValueDW(blacklist::kBeaconState,
                                                &blacklist_state);
  EXPECT_EQ(ERROR_SUCCESS, result);
  EXPECT_EQ(blacklist_state,
            static_cast<DWORD>(blacklist::BLACKLIST_SETUP_FAILED));
}

TEST_F(BlacklistTest, SetupSucceeded) {
  // Starting with the enabled beacon should result in the setup running state
  // and the attempt counter reset to zero.
  LONG result = blacklist_registry_key_->WriteValue(
      blacklist::kBeaconState, blacklist::BLACKLIST_ENABLED);
  EXPECT_EQ(ERROR_SUCCESS, result);
  result = blacklist_registry_key_->WriteValue(blacklist::kBeaconAttemptCount,
                                               blacklist::kBeaconMaxAttempts);
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_TRUE(blacklist::LeaveSetupBeacon());

  DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
  blacklist_registry_key_->ReadValueDW(blacklist::kBeaconState,
                                       &blacklist_state);
  EXPECT_EQ(blacklist_state,
            static_cast<DWORD>(blacklist::BLACKLIST_SETUP_RUNNING));

  DWORD attempt_count = blacklist::kBeaconMaxAttempts;
  blacklist_registry_key_->ReadValueDW(blacklist::kBeaconAttemptCount,
                                       &attempt_count);
  EXPECT_EQ(static_cast<DWORD>(0), attempt_count);
}

}  // namespace
