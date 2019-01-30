// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"

#include <windows.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/pe_image.h"
#include "base/win/registry.h"
#include "chrome/browser/conflicts/module_blacklist_cache_util_win.h"
#include "chrome/browser/conflicts/module_info_win.h"
#include "chrome/browser/conflicts/module_list_filter_win.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Stubs an empty whitelist and blacklist.
class StubModuleListFilter : public ModuleListFilter {
 public:
  StubModuleListFilter() = default;
  ~StubModuleListFilter() override = default;

  bool IsWhitelisted(base::StringPiece basename_hash,
                     base::StringPiece code_id_hash) const override {
    return false;
  }

  std::unique_ptr<chrome::conflicts::BlacklistAction> IsBlacklisted(
      const ModuleInfoKey& module_key,
      const ModuleInfoData& module_data) const override {
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StubModuleListFilter);
};

constexpr base::FilePath::CharType kCertificatePath[] =
    FILE_PATH_LITERAL("CertificatePath");
constexpr base::FilePath::CharType kCertificateSubject[] =
    FILE_PATH_LITERAL("CertificateSubject");

constexpr base::FilePath::CharType kDllPath1[] =
    FILE_PATH_LITERAL("c:\\path\\to\\module.dll");
constexpr base::FilePath::CharType kDllPath2[] =
    FILE_PATH_LITERAL("c:\\some\\shellextension.dll");

// Returns a new ModuleInfoData marked as loaded into the process but otherwise
// empty.
ModuleInfoData CreateLoadedModuleInfoData() {
  ModuleInfoData module_data;
  module_data.module_types |= ModuleInfoData::kTypeLoadedModule;
  module_data.inspection_result = std::make_unique<ModuleInspectionResult>();
  return module_data;
}

// Returns a new ModuleInfoData marked as loaded into the process with a
// CertificateInfo that matches kCertificateSubject.
ModuleInfoData CreateSignedLoadedModuleInfoData() {
  ModuleInfoData module_data = CreateLoadedModuleInfoData();

  module_data.inspection_result->certificate_info.type =
      CertificateType::CERTIFICATE_IN_FILE;
  module_data.inspection_result->certificate_info.path =
      base::FilePath(kCertificatePath);
  module_data.inspection_result->certificate_info.subject = kCertificateSubject;

  return module_data;
}

void GetModulePath(HMODULE module_handle, base::FilePath* module_path) {
  base::FilePath result;

  wchar_t buffer[MAX_PATH];
  DWORD length = ::GetModuleFileName(module_handle, buffer, MAX_PATH);
  ASSERT_NE(length, 0U);
  ASSERT_LT(length, static_cast<DWORD>(MAX_PATH));

  *module_path = base::FilePath(buffer);
}

}  // namespace

class ModuleBlacklistCacheUpdaterTest : public testing::Test,
                                        public ModuleDatabaseEventSource {
 protected:
  ModuleBlacklistCacheUpdaterTest()
      : dll1_(kDllPath1),
        dll2_(kDllPath2),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        user_data_dir_override_(chrome::DIR_USER_DATA),
        module_blacklist_cache_path_(
            ModuleBlacklistCacheUpdater::GetModuleBlacklistCachePath()) {
    exe_certificate_info_.type = CertificateType::CERTIFICATE_IN_FILE;
    exe_certificate_info_.path = base::FilePath(kCertificatePath);
    exe_certificate_info_.subject = kCertificateSubject;
  }

  void SetUp() override {
    ASSERT_TRUE(base::CreateDirectory(module_blacklist_cache_path().DirName()));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  std::unique_ptr<ModuleBlacklistCacheUpdater>
  CreateModuleBlacklistCacheUpdater() {
    return std::make_unique<ModuleBlacklistCacheUpdater>(
        this, exe_certificate_info_, module_list_filter_,
        base::BindRepeating(
            &ModuleBlacklistCacheUpdaterTest::OnModuleBlacklistCacheUpdated,
            base::Unretained(this)));
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) {
    scoped_task_environment_.FastForwardBy(delta);
    // The expired timer callback posts a task to update the cache. Wait for it
    // to finish.
    scoped_task_environment_.RunUntilIdle();
  }

  base::FilePath& module_blacklist_cache_path() {
    return module_blacklist_cache_path_;
  }

  bool on_cache_updated_callback_invoked() {
    return on_cache_updated_callback_invoked_;
  }

  // ModuleDatabaseEventSource:
  void AddObserver(ModuleDatabaseObserver* observer) override {}
  void RemoveObserver(ModuleDatabaseObserver* observer) override {}

  const base::FilePath dll1_;
  const base::FilePath dll2_;

 private:
  void OnModuleBlacklistCacheUpdated(
      const ModuleBlacklistCacheUpdater::CacheUpdateResult& result) {
    on_cache_updated_callback_invoked_ = true;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  base::ScopedPathOverride user_data_dir_override_;

  CertificateInfo exe_certificate_info_;
  StubModuleListFilter module_list_filter_;

  base::FilePath module_blacklist_cache_path_;

  bool on_cache_updated_callback_invoked_ = false;

  DISALLOW_COPY_AND_ASSIGN(ModuleBlacklistCacheUpdaterTest);
};

TEST_F(ModuleBlacklistCacheUpdaterTest, OneThirdPartyModule) {
  EXPECT_FALSE(base::PathExists(module_blacklist_cache_path()));

  auto module_blacklist_cache_updater = CreateModuleBlacklistCacheUpdater();

  // Simulate some arbitrary module loading into the process.
  module_blacklist_cache_updater->OnNewModuleFound(
      ModuleInfoKey(dll1_, 0, 0, 0), CreateLoadedModuleInfoData());
  module_blacklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blacklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlacklistCache(module_blacklist_cache_path(), &metadata,
                                     &blacklisted_modules, &md5_digest));

  EXPECT_EQ(1u, blacklisted_modules.size());
}

TEST_F(ModuleBlacklistCacheUpdaterTest, IgnoreMicrosoftModules) {
  EXPECT_FALSE(base::PathExists(module_blacklist_cache_path()));

  // base::RunLoop run_loop;
  auto module_blacklist_cache_updater = CreateModuleBlacklistCacheUpdater();

  // Simulate a Microsoft module loading into the process.
  base::win::PEImage kernel32_image(::GetModuleHandle(L"kernel32.dll"));
  ASSERT_TRUE(kernel32_image.module());

  base::FilePath module_path;
  ASSERT_NO_FATAL_FAILURE(GetModulePath(kernel32_image.module(), &module_path));
  ASSERT_FALSE(module_path.empty());
  uint32_t module_size =
      kernel32_image.GetNTHeaders()->OptionalHeader.SizeOfImage;
  uint32_t time_date_stamp =
      kernel32_image.GetNTHeaders()->FileHeader.TimeDateStamp;

  ModuleInfoKey module_key(module_path, module_size, time_date_stamp, 0);
  ModuleInfoData module_data = CreateLoadedModuleInfoData();
  module_data.inspection_result = InspectModule(StringMapping(), module_key);

  module_blacklist_cache_updater->OnNewModuleFound(module_key, module_data);
  module_blacklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blacklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlacklistCache(module_blacklist_cache_path(), &metadata,
                                     &blacklisted_modules, &md5_digest));

  EXPECT_EQ(0u, blacklisted_modules.size());
}

// Tests that modules with a matching certificate subject are whitelisted.
TEST_F(ModuleBlacklistCacheUpdaterTest, WhitelistMatchingCertificateSubject) {
  EXPECT_FALSE(base::PathExists(module_blacklist_cache_path()));

  auto module_blacklist_cache_updater = CreateModuleBlacklistCacheUpdater();

  // Simulate the module loading into the process.
  module_blacklist_cache_updater->OnNewModuleFound(
      ModuleInfoKey(dll1_, 0, 0, 0), CreateSignedLoadedModuleInfoData());
  module_blacklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blacklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlacklistCache(module_blacklist_cache_path(), &metadata,
                                     &blacklisted_modules, &md5_digest));

  EXPECT_EQ(0u, blacklisted_modules.size());
}

// Make sure IMEs are allowed while shell extensions are blacklisted.
TEST_F(ModuleBlacklistCacheUpdaterTest, RegisteredModules) {
  EXPECT_FALSE(base::PathExists(module_blacklist_cache_path()));

  auto module_blacklist_cache_updater = CreateModuleBlacklistCacheUpdater();

  // Set the respective bit for registered modules.
  ModuleInfoKey module_key1(dll1_, 123u, 456u, 0);
  ModuleInfoData module_data1 = CreateLoadedModuleInfoData();
  module_data1.module_types |= ModuleInfoData::kTypeIme;

  ModuleInfoKey module_key2(dll2_, 456u, 789u, 0);
  ModuleInfoData module_data2 = CreateLoadedModuleInfoData();
  module_data2.module_types |= ModuleInfoData::kTypeShellExtension;

  // Simulate the modules loading into the process.
  module_blacklist_cache_updater->OnNewModuleFound(module_key1, module_data1);
  module_blacklist_cache_updater->OnNewModuleFound(module_key2, module_data2);
  module_blacklist_cache_updater->OnModuleDatabaseIdle();

  RunUntilIdle();
  EXPECT_TRUE(base::PathExists(module_blacklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());

  // Check the cache.
  third_party_dlls::PackedListMetadata metadata;
  std::vector<third_party_dlls::PackedListModule> blacklisted_modules;
  base::MD5Digest md5_digest;
  EXPECT_EQ(ReadResult::kSuccess,
            ReadModuleBlacklistCache(module_blacklist_cache_path(), &metadata,
                                     &blacklisted_modules, &md5_digest));

  // Make sure the only blacklisted module is the shell extension.
  ASSERT_EQ(1u, blacklisted_modules.size());

  third_party_dlls::PackedListModule expected;
  const std::string module_basename = base::UTF16ToUTF8(
      base::i18n::ToLower(module_key2.module_path.BaseName().value()));
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(module_basename.data()),
                      module_basename.length(), expected.basename_hash);
  const std::string module_code_id = GenerateCodeId(module_key2);
  base::SHA1HashBytes(reinterpret_cast<const uint8_t*>(module_code_id.data()),
                      module_code_id.length(), expected.code_id_hash);

  EXPECT_TRUE(internal::ModuleEqual()(expected, blacklisted_modules[0]));
}

// This tests that if a new blocked load attempt arrives, an update will still
// be triggered even if the Module Database never goes idle afterwards.
TEST_F(ModuleBlacklistCacheUpdaterTest, NewModulesBlockedOnly) {
  EXPECT_FALSE(base::PathExists(module_blacklist_cache_path()));

  auto module_blacklist_cache_updater = CreateModuleBlacklistCacheUpdater();

  // Simulate a new blocked load attempt.
  std::vector<third_party_dlls::PackedListModule> blocked_modules;
  const third_party_dlls::PackedListModule module = {
      {}, {}, 123456u,
  };
  blocked_modules.push_back(module);
  module_blacklist_cache_updater->OnNewModulesBlocked(
      std::move(blocked_modules));

  FastForwardBy(ModuleBlacklistCacheUpdater::kUpdateTimerDuration);
  EXPECT_TRUE(base::PathExists(module_blacklist_cache_path()));
  EXPECT_TRUE(on_cache_updated_callback_invoked());
}
