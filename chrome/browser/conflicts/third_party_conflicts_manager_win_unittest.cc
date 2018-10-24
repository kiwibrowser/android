// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/third_party_conflicts_manager_win.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/conflicts/proto/module_list.pb.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class ThirdPartyConflictsManagerTest : public testing::Test,
                                       public ModuleDatabaseEventSource {
 public:
  ThirdPartyConflictsManagerTest()
      : scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    scoped_feature_list_.InitWithFeatures(
        // Enabled features.
        {features::kIncompatibleApplicationsWarning,
         features::kThirdPartyModulesBlocking},
        // Disabled features.
        {});
  }

  // Returns the path to the module list.
  base::FilePath GetModuleListPath() const {
    return scoped_temp_dir_.GetPath().Append(L"ModuleList.bin");
  }

  // Writes an empty serialized ModuleList proto to |GetModuleListPath()|.
  void CreateModuleList() {
    chrome::conflicts::ModuleList module_list;
    // Include an empty blacklist and whitelist.
    module_list.mutable_blacklist();
    module_list.mutable_whitelist();

    std::string contents;
    ASSERT_TRUE(module_list.SerializeToString(&contents));
    ASSERT_EQ(base::WriteFile(GetModuleListPath(), contents.data(),
                              static_cast<int>(contents.size())),
              static_cast<int>(contents.size()));
  }

  void OnManagerInitializationComplete(
      base::Closure quit_closure,
      ThirdPartyConflictsManager::State final_state) {
    final_state_ = final_state;
    std::move(quit_closure).Run();
  }

  const base::Optional<ThirdPartyConflictsManager::State>& final_state() {
    return final_state_;
  }

  // ModuleDatabaseEventSource:
  void AddObserver(ModuleDatabaseObserver* observer) override {}
  void RemoveObserver(ModuleDatabaseObserver* observer) override {}

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  ScopedTestingLocalState scoped_testing_local_state_;

  // Temp directory used to host module list.
  base::ScopedTempDir scoped_temp_dir_;

  base::test::ScopedFeatureList scoped_feature_list_;

  base::Optional<ThirdPartyConflictsManager::State> final_state_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyConflictsManagerTest);
};

TEST_F(ThirdPartyConflictsManagerTest, InitializeBothUpdaters) {
  ThirdPartyConflictsManager third_party_conflicts_manager(this);

  third_party_conflicts_manager.OnModuleDatabaseIdle();
  ASSERT_NO_FATAL_FAILURE(CreateModuleList());
  third_party_conflicts_manager.LoadModuleList(GetModuleListPath());

  base::RunLoop run_loop;
  third_party_conflicts_manager.ForceInitialization(base::BindRepeating(
      &ThirdPartyConflictsManagerTest::OnManagerInitializationComplete,
      base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_TRUE(final_state().has_value());
  EXPECT_EQ(final_state().value(),
            ThirdPartyConflictsManager::State::kInitialized);
}

TEST_F(ThirdPartyConflictsManagerTest, InvalidModuleList) {
  ThirdPartyConflictsManager third_party_conflicts_manager(this);

  third_party_conflicts_manager.OnModuleDatabaseIdle();

  // Pass in an empty path which will ensure that the deserialization will fail.
  third_party_conflicts_manager.LoadModuleList(GetModuleListPath());

  base::RunLoop run_loop;
  third_party_conflicts_manager.ForceInitialization(base::BindRepeating(
      &ThirdPartyConflictsManagerTest::OnManagerInitializationComplete,
      base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();

  ASSERT_TRUE(final_state().has_value());
  EXPECT_EQ(final_state().value(),
            ThirdPartyConflictsManager::State::kModuleListInvalidFailure);
}

TEST_F(ThirdPartyConflictsManagerTest, DestroyManager) {
  auto third_party_conflicts_manager =
      std::make_unique<ThirdPartyConflictsManager>(this);

  third_party_conflicts_manager->OnModuleDatabaseIdle();
  ASSERT_NO_FATAL_FAILURE(CreateModuleList());
  third_party_conflicts_manager->LoadModuleList(GetModuleListPath());

  base::RunLoop run_loop;
  third_party_conflicts_manager->ForceInitialization(base::BindRepeating(
      &ThirdPartyConflictsManagerTest::OnManagerInitializationComplete,
      base::Unretained(this), run_loop.QuitClosure()));

  // Delete the instance while it is initializing.
  third_party_conflicts_manager = nullptr;
  run_loop.Run();

  ASSERT_TRUE(final_state().has_value());
  EXPECT_EQ(final_state().value(),
            ThirdPartyConflictsManager::State::kDestroyed);
}
