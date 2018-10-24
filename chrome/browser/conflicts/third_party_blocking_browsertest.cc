// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"
#include "chrome/browser/conflicts/module_blacklist_cache_util_win.h"
#include "chrome/browser/conflicts/module_database_win.h"
#include "chrome/browser/conflicts/proto/module_list.pb.h"
#include "chrome/browser/conflicts/third_party_conflicts_manager_win.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome_elf/third_party_dlls/packed_list_format.h"

namespace {

// This classes watches the module blacklist cache directory to detect when the
// cache file is created.
class ModuleBlacklistCacheObserver {
 public:
  ModuleBlacklistCacheObserver(
      const base::FilePath& module_blacklist_cache_path)
      : module_blacklist_cache_path_(module_blacklist_cache_path) {}

  bool StartWatching() {
    return file_path_watcher_.Watch(
        module_blacklist_cache_path_.DirName(), false,
        base::Bind(
            &ModuleBlacklistCacheObserver::OnModuleBlacklistCachePathChanged,
            base::Unretained(this)));
  }

  void WaitForModuleBlacklistCacheCreated() {
    if (module_blacklist_cache_created_)
      return;

    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnModuleBlacklistCachePathChanged(const base::FilePath& path,
                                         bool error) {
    if (!base::PathExists(module_blacklist_cache_path_))
      return;

    module_blacklist_cache_created_ = true;

    if (run_loop_quit_closure_)
      std::move(run_loop_quit_closure_).Run();
  }

  // Needed to watch a file on main thread.
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_;

  // The path to the module_blacklist_cache.
  base::FilePath module_blacklist_cache_path_;

  base::FilePathWatcher file_path_watcher_;

  // Remembers if the cache was created in case the callback is invoked before
  // WaitForModuleBlacklistCacheCreated() was called.
  bool module_blacklist_cache_created_ = false;

  base::Closure run_loop_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ModuleBlacklistCacheObserver);
};

class ThirdPartyBlockingBrowserTest : public InProcessBrowserTest {
 protected:
  ThirdPartyBlockingBrowserTest() = default;

  ~ThirdPartyBlockingBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kThirdPartyModulesBlocking);

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());

    InProcessBrowserTest::SetUp();
  }

  // Creates a copy of a test DLL into a temp directory that will act as the
  // third-party module and return its path. It can't be located in the output
  // directory because modules in the same directory as chrome.exe are
  // whitelisted in non-official builds.
  void CreateThirdPartyModule(base::FilePath* third_party_module_path) {
    base::FilePath test_dll_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_dll_path));
    test_dll_path =
        test_dll_path.Append(FILE_PATH_LITERAL("conflicts_dll.dll"));
    *third_party_module_path = scoped_temp_dir_.GetPath().Append(
        FILE_PATH_LITERAL("third_party_module.dll"));
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    ASSERT_TRUE(base::CopyFile(test_dll_path, *third_party_module_path));
  }

  // Creates an empty serialized ModuleList proto in the module list component
  // directory and returns its path.
  void CreateModuleList(base::FilePath* module_list_path) {
    chrome::conflicts::ModuleList module_list;
    // Include an empty blacklist and whitelist.
    module_list.mutable_blacklist();
    module_list.mutable_whitelist();

    std::string contents;
    ASSERT_TRUE(module_list.SerializeToString(&contents));

    // Put the module list beside the module blacklist cache.
    *module_list_path =
        ModuleBlacklistCacheUpdater::GetModuleBlacklistCachePath()
            .DirName()
            .Append(FILE_PATH_LITERAL("ModuleList.bin"));

    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    ASSERT_TRUE(base::CreateDirectory(module_list_path->DirName()));
    ASSERT_EQ(static_cast<int>(contents.size()),
              base::WriteFile(*module_list_path, contents.data(),
                              static_cast<int>(contents.size())));
  }

  // Enables the ThirdPartyModulesBlocking feature.
  base::test::ScopedFeatureList scoped_feature_list_;

  // Temp directory where the third-party module is located.
  base::ScopedTempDir scoped_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyBlockingBrowserTest);
};

}  // namespace

// This is an integration test for the blocking of third-party modules.
//
// This test makes sure that all the different classes interact together
// correctly to produce a valid module blacklist cache.
//
// Note: This doesn't test that the modules are actually blocked on the next
//       browser launch.
IN_PROC_BROWSER_TEST_F(ThirdPartyBlockingBrowserTest,
                       CreateModuleBlacklistCache) {
  base::FilePath module_list_path;
  ASSERT_NO_FATAL_FAILURE(CreateModuleList(&module_list_path));
  ASSERT_FALSE(module_list_path.empty());

  ModuleDatabase* module_database = ModuleDatabase::GetInstance();

  // Speed up the test.
  module_database->IncreaseInspectionPriority();

  base::FilePath module_blacklist_cache_path =
      ModuleBlacklistCacheUpdater::GetModuleBlacklistCachePath();
  ASSERT_FALSE(module_blacklist_cache_path.empty());

  // Create the observer early so the change is guaranteed to be observed.
  ModuleBlacklistCacheObserver module_blacklist_cache_observer(
      module_blacklist_cache_path);
  ASSERT_TRUE(module_blacklist_cache_observer.StartWatching());

  // Simulate the download of the module list component.
  module_database->third_party_conflicts_manager()->LoadModuleList(
      module_list_path);

  // Injects the third-party DLL into the process.
  base::FilePath third_party_module_path;
  ASSERT_NO_FATAL_FAILURE(CreateThirdPartyModule(&third_party_module_path));
  ASSERT_FALSE(third_party_module_path.empty());

  base::ScopedNativeLibrary dll(third_party_module_path);
  ASSERT_TRUE(dll.is_valid());

  // Now the module blacklist cache will eventually be created.
  module_blacklist_cache_observer.WaitForModuleBlacklistCacheCreated();

  // Now check that the third-party DLL was added to the module blacklist cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  ASSERT_EQ(ReadResult::kSuccess,
            ReadModuleBlacklistCache(module_blacklist_cache_path, &metadata,
                                     &blacklisted_modules, &md5_digest));

  EXPECT_GE(blacklisted_modules.size(), 1);
}
