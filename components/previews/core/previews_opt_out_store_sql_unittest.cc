// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_opt_out_store_sql.h"

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/previews/core/blacklist_data.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_opt_out_store.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

namespace {

const base::FilePath::CharType kOptOutFilename[] = FILE_PATH_LITERAL("OptOut");

}  // namespace

class PreviewsOptOutStoreSQLTest : public testing::Test {
 public:
  PreviewsOptOutStoreSQLTest()
      : field_trials_(new base::FieldTrialList(nullptr)) {}
  ~PreviewsOptOutStoreSQLTest() override {}

  // Called when |store_| is done loading.
  void OnLoaded(std::unique_ptr<BlacklistData> blacklist_data) {
    blacklist_data_ = std::move(blacklist_data);
  }

  // Initializes the store and get the data from it.
  void Load() {
    std::unique_ptr<BlacklistData> data = std::make_unique<BlacklistData>(
        std::make_unique<BlacklistData::Policy>(params::SingleOptOutDuration(),
                                                1, 1),
        std::make_unique<BlacklistData::Policy>(
            params::HostIndifferentBlackListPerHostDuration(),
            params::MaxStoredHistoryLengthForHostIndifferentBlackList(),
            params::HostIndifferentBlackListOptOutThreshold()),
        std::make_unique<BlacklistData::Policy>(
            params::PerHostBlackListDuration(),
            params::MaxStoredHistoryLengthForPerHostBlackList(),
            params::PerHostBlackListOptOutThreshold()),
        nullptr, params::MaxInMemoryHostsInBlackList(), enabled_previews_);

    store_->LoadBlackList(std::move(data),
                          base::BindOnce(&PreviewsOptOutStoreSQLTest::OnLoaded,
                                         base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  // Destroys the database connection and |store_|.
  void DestroyStore() {
    store_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Creates a store that operates on one thread.
  void Create() {
    store_ = std::make_unique<PreviewsOptOutStoreSQL>(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        temp_dir_.GetPath().Append(kOptOutFilename));
  }

  // Sets up initialization of |store_|.
  void CreateAndLoad() {
    Create();
    Load();
  }

  void SetEnabledTypes(
      BlacklistData::AllowedTypesAndVersions enabled_previews) {
    enabled_previews_ = std::move(enabled_previews);
  }

  // Creates a directory for the test.
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Delete |store_| if it hasn't been deleted.
  void TearDown() override { DestroyStore(); }

 protected:
  void ResetFieldTrials() {
    // Destroy existing FieldTrialList before creating new one to avoid DCHECK.
    field_trials_.reset();
    field_trials_.reset(new base::FieldTrialList(nullptr));
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::MessageLoop message_loop_;

  // The backing SQL store.
  std::unique_ptr<PreviewsOptOutStoreSQL> store_;

  // The map returned from |store_|.
  std::unique_ptr<BlacklistData> blacklist_data_;

  // The directory for the database.
  base::ScopedTempDir temp_dir_;

 private:
  std::unique_ptr<base::FieldTrialList> field_trials_;

  BlacklistData::AllowedTypesAndVersions enabled_previews_;
};

TEST_F(PreviewsOptOutStoreSQLTest, TestErrorRecovery) {
  // Creates the database and corrupt to test the recovery method.
  std::string test_host = "host.com";
  BlacklistData::AllowedTypesAndVersions enabled_previews;
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  store_->AddEntry(true, test_host, static_cast<int>(PreviewsType::OFFLINE),
                   base::Time::Now());
  base::RunLoop().RunUntilIdle();
  DestroyStore();

  // Corrupts the database by adjusting the header size.
  EXPECT_TRUE(sql::test::CorruptSizeInHeader(
      temp_dir_.GetPath().Append(kOptOutFilename)));
  base::RunLoop().RunUntilIdle();

  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  // The data should be recovered.
  EXPECT_EQ(1U, blacklist_data_->black_list_item_host_map().size());
  const auto& iter =
      blacklist_data_->black_list_item_host_map().find(test_host);

  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());
}

TEST_F(PreviewsOptOutStoreSQLTest, TestPersistance) {
  // Tests if data is stored as expected in the SQLite database.
  std::string test_host = "host.com";
  BlacklistData::AllowedTypesAndVersions enabled_previews;
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  base::Time now = base::Time::Now();
  store_->AddEntry(true, test_host, static_cast<int>(PreviewsType::OFFLINE),
                   now);
  base::RunLoop().RunUntilIdle();

  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk.
  DestroyStore();

  // Reload and test for persistence
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  EXPECT_EQ(1U, blacklist_data_->black_list_item_host_map().size());
  const auto& iter =
      blacklist_data_->black_list_item_host_map().find(test_host);

  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());
  EXPECT_EQ(now, iter->second.most_recent_opt_out_time().value());
}

TEST_F(PreviewsOptOutStoreSQLTest, TestMaxRows) {
  // Tests that the number of rows are culled down to the row limit at each
  // load.
  std::string test_host_a = "host_a.com";
  std::string test_host_b = "host_b.com";
  std::string test_host_c = "host_c.com";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  size_t row_limit = 2;
  std::string row_limit_string = base::NumberToString(row_limit);
  command_line->AppendSwitchASCII("previews-max-opt-out-rows",
                                  row_limit_string);
  BlacklistData::AllowedTypesAndVersions enabled_previews;
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  base::SimpleTestClock clock;

  // Create three different entries with different hosts.
  store_->AddEntry(true, test_host_a, static_cast<int>(PreviewsType::OFFLINE),
                   clock.Now());
  clock.Advance(base::TimeDelta::FromSeconds(1));

  store_->AddEntry(true, test_host_b, static_cast<int>(PreviewsType::OFFLINE),
                   clock.Now());
  base::Time host_b_time = clock.Now();
  clock.Advance(base::TimeDelta::FromSeconds(1));

  store_->AddEntry(false, test_host_c, static_cast<int>(PreviewsType::OFFLINE),
                   clock.Now());
  base::RunLoop().RunUntilIdle();
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk.
  DestroyStore();

  // Reload and test for persistence
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  // The delete happens after the load, so it is possible to load more than
  // |row_limit| into the in memory map.
  EXPECT_EQ(row_limit + 1, blacklist_data_->black_list_item_host_map().size());

  DestroyStore();
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();

  EXPECT_EQ(row_limit, blacklist_data_->black_list_item_host_map().size());
  const auto& iter_host_b =
      blacklist_data_->black_list_item_host_map().find(test_host_b);
  const auto& iter_host_c =
      blacklist_data_->black_list_item_host_map().find(test_host_c);

  EXPECT_EQ(blacklist_data_->black_list_item_host_map().end(),
            blacklist_data_->black_list_item_host_map().find(test_host_a));
  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter_host_b);
  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter_host_c);
  EXPECT_EQ(host_b_time,
            iter_host_b->second.most_recent_opt_out_time().value());
  EXPECT_EQ(1U, iter_host_b->second.OptOutRecordsSizeForTesting());
}

TEST_F(PreviewsOptOutStoreSQLTest, TestMaxRowsPerHost) {
  // Tests that each host is limited to |row_limit| rows.
  std::string test_host = "host.com";
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  size_t row_limit = 2;
  std::string row_limit_string = base::NumberToString(row_limit);
  command_line->AppendSwitchASCII("previews-max-opt-out-rows-per-host",
                                  row_limit_string);
  BlacklistData::AllowedTypesAndVersions enabled_previews;
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  base::SimpleTestClock clock;

  base::Time last_opt_out_time;
  for (size_t i = 0; i < row_limit; i++) {
    store_->AddEntry(true, test_host, static_cast<int>(PreviewsType::OFFLINE),
                     clock.Now());
    last_opt_out_time = clock.Now();
    clock.Advance(base::TimeDelta::FromSeconds(1));
  }

  clock.Advance(base::TimeDelta::FromSeconds(1));
  store_->AddEntry(false, test_host, static_cast<int>(PreviewsType::OFFLINE),
                   clock.Now());

  base::RunLoop().RunUntilIdle();
  // Replace the store effectively destroying the current one and forcing it
  // to write its data to disk.
  DestroyStore();

  // Reload and test for persistence.
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();

  EXPECT_EQ(1U, blacklist_data_->black_list_item_host_map().size());
  const auto& iter =
      blacklist_data_->black_list_item_host_map().find(test_host);

  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter);
  EXPECT_EQ(last_opt_out_time, iter->second.most_recent_opt_out_time().value());
  EXPECT_EQ(row_limit, iter->second.OptOutRecordsSizeForTesting());
  clock.Advance(base::TimeDelta::FromSeconds(1));
  // If both entries' opt out states are stored correctly, then this should not
  // be black listed.
  EXPECT_FALSE(iter->second.IsBlackListed(clock.Now()));
}

TEST_F(PreviewsOptOutStoreSQLTest, TestPreviewsDisabledClearsBlacklistEntry) {
  // Tests if data is cleared for previews type when it is disabled.
  // Enable OFFLINE previews and add black list entry for it.
  std::map<std::string, std::string> params;
  std::string test_host = "host.com";
  BlacklistData::AllowedTypesAndVersions enabled_previews;
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  base::Time now = base::Time::Now();
  store_->AddEntry(true, test_host, static_cast<int>(PreviewsType::OFFLINE),
                   now);
  base::RunLoop().RunUntilIdle();

  // Force data write to database then reload it and verify black list entry
  // is present.
  DestroyStore();
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  const auto& iter =
      blacklist_data_->black_list_item_host_map().find(test_host);
  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());

  DestroyStore();
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::LOFI), 0});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  const auto& iter2 =
      blacklist_data_->black_list_item_host_map().find(test_host);
  EXPECT_EQ(blacklist_data_->black_list_item_host_map().end(), iter2);

  DestroyStore();
  enabled_previews.clear();
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  const auto& iter3 =
      blacklist_data_->black_list_item_host_map().find(test_host);
  EXPECT_EQ(blacklist_data_->black_list_item_host_map().end(), iter3);
}

TEST_F(PreviewsOptOutStoreSQLTest,
       TestPreviewsVersionUpdateClearsBlacklistEntry) {
  // Tests if data is cleared for new version of previews type.
  // Enable OFFLINE previews and add black list entry for it.
  std::string test_host = "host.com";
  BlacklistData::AllowedTypesAndVersions enabled_previews;
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 1});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  base::Time now = base::Time::Now();
  store_->AddEntry(true, test_host, static_cast<int>(PreviewsType::OFFLINE),
                   now);
  base::RunLoop().RunUntilIdle();

  // Force data write to database then reload it and verify black list entry
  // is present.
  DestroyStore();
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 1});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  const auto& iter =
      blacklist_data_->black_list_item_host_map().find(test_host);
  EXPECT_NE(blacklist_data_->black_list_item_host_map().end(), iter);
  EXPECT_EQ(1U, iter->second.OptOutRecordsSizeForTesting());

  DestroyStore();
  enabled_previews.clear();
  enabled_previews.insert({static_cast<int>(PreviewsType::OFFLINE), 2});
  SetEnabledTypes(std::move(enabled_previews));
  CreateAndLoad();
  const auto& iter2 =
      blacklist_data_->black_list_item_host_map().find(test_host);
  EXPECT_EQ(blacklist_data_->black_list_item_host_map().end(), iter2);
}

}  // namespace net
