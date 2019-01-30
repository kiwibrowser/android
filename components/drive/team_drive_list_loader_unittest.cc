// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "components/drive/chromeos/change_list_loader.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/loader_controller.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/chromeos/team_drive_list_loader.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

constexpr char kTestStartPageToken[] = "123456";

class TestTeamDriveListObserver : public TeamDriveListObserver {
 public:
  ~TestTeamDriveListObserver() override = default;

  void OnTeamDriveListLoaded(
      const std::vector<TeamDrive>& team_drives_list,
      const std::vector<TeamDrive>& added_team_drives,
      const std::vector<TeamDrive>& removed_team_drives) override {
    team_drives_list_ = team_drives_list;
    added_team_drives_ = added_team_drives;
    removed_team_drives_ = removed_team_drives;
  }

  const std::vector<TeamDrive>& team_drives_list() const {
    return team_drives_list_;
  }

  const std::vector<TeamDrive>& added_team_drives() const {
    return added_team_drives_;
  }

  const std::vector<TeamDrive>& removed_team_drives() const {
    return removed_team_drives_;
  }

 private:
  std::vector<TeamDrive> team_drives_list_;
  std::vector<TeamDrive> added_team_drives_;
  std::vector<TeamDrive> removed_team_drives_;
};

}  // namespace

class TeamDriveListLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_util::RegisterDrivePrefs(pref_service_->registry());

    logger_ = std::make_unique<EventLogger>();

    drive_service_ = std::make_unique<FakeDriveService>();
    ASSERT_TRUE(test_util::SetUpTestEntries(drive_service_.get()));
    drive_service_->set_default_max_results(2);

    scheduler_ = std::make_unique<JobScheduler>(
        pref_service_.get(), logger_.get(), drive_service_.get(),
        base::ThreadTaskRunnerHandle::Get().get(), nullptr);
    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(), temp_dir_.GetPath(),
                               base::ThreadTaskRunnerHandle::Get().get(),
                               nullptr /* free_disk_space_getter */));
    ASSERT_TRUE(cache_->Initialize());

    metadata_.reset(
        new ResourceMetadata(metadata_storage_.get(), cache_.get(),
                             base::ThreadTaskRunnerHandle::Get().get()));
    ASSERT_EQ(FILE_ERROR_OK, metadata_->Initialize());

    loader_controller_ = std::make_unique<LoaderController>();
    team_drive_list_loader_ = std::make_unique<TeamDriveListLoader>(
        logger_.get(), base::ThreadTaskRunnerHandle::Get().get(),
        metadata_.get(), scheduler_.get(), loader_controller_.get());

    team_drive_list_observer_ = std::make_unique<TestTeamDriveListObserver>();
    team_drive_list_loader_->AddObserver(team_drive_list_observer_.get());
  }

  // Creates a ResourceEntry for a directory with explicitly set resource_id.
  ResourceEntry CreateDirectoryEntryWithResourceId(
      const std::string& title,
      const std::string& resource_id,
      const std::string& parent_local_id) {
    ResourceEntry entry;
    entry.set_title(title);
    entry.set_resource_id(resource_id);
    entry.set_parent_local_id(parent_local_id);
    entry.mutable_file_info()->set_is_directory(true);
    entry.mutable_directory_specific_info()->set_start_page_token(
        kTestStartPageToken);
    return entry;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<EventLogger> logger_;
  std::unique_ptr<FakeDriveService> drive_service_;
  std::unique_ptr<JobScheduler> scheduler_;
  std::unique_ptr<ResourceMetadataStorage, test_util::DestroyHelperForTests>
      metadata_storage_;
  std::unique_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
  std::unique_ptr<ResourceMetadata, test_util::DestroyHelperForTests> metadata_;
  std::unique_ptr<LoaderController> loader_controller_;
  std::unique_ptr<TeamDriveListLoader> team_drive_list_loader_;
  std::unique_ptr<TestTeamDriveListObserver> team_drive_list_observer_;
};

// Tests that if there are no team drives on the server, we will not add
// and team drives to local metadata.
TEST_F(TeamDriveListLoaderTest, NoTeamDrives) {
  // Tests the response if there are no team drives loaded.
  FileError error = FILE_ERROR_FAILED;
  team_drive_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1, drive_service_->team_drive_list_load_count());

  EXPECT_TRUE(team_drive_list_observer_->team_drives_list().empty());
  EXPECT_TRUE(team_drive_list_observer_->added_team_drives().empty());
  EXPECT_TRUE(team_drive_list_observer_->removed_team_drives().empty());

  ResourceEntryVector entries;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->ReadDirectoryByPath(
                               util::GetDriveTeamDrivesRootPath(), &entries));
  EXPECT_TRUE(entries.empty());
}

// Tests if there is one team drive on the server we will add it to the local
// metadata.
TEST_F(TeamDriveListLoaderTest, OneTeamDrive) {
  constexpr char kTeamDriveId1[] = "the1stTeamDriveId";
  constexpr char kTeamDriveName1[] = "The First Team Drive";
  drive_service_->AddTeamDrive(kTeamDriveId1, kTeamDriveName1);

  FileError error = FILE_ERROR_FAILED;
  team_drive_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1, drive_service_->team_drive_list_load_count());

  EXPECT_EQ(1UL, team_drive_list_observer_->team_drives_list().size());
  EXPECT_EQ(1UL, team_drive_list_observer_->added_team_drives().size());
  EXPECT_TRUE(team_drive_list_observer_->removed_team_drives().empty());

  ResourceEntryVector entries;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->ReadDirectoryByPath(
                               util::GetDriveTeamDrivesRootPath(), &entries));
  EXPECT_EQ(1UL, entries.size());

  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(
                util::GetDriveTeamDrivesRootPath().AppendASCII(kTeamDriveName1),
                &entry));

  const base::FilePath& team_drive_path =
      team_drive_list_observer_->team_drives_list().front().team_drive_path();
  EXPECT_EQ(team_drive_path,
            util::GetDriveTeamDrivesRootPath().AppendASCII(kTeamDriveName1));
}

// Tests if there are multiple team drives on the server, we will add them all
// to local metadata.
TEST_F(TeamDriveListLoaderTest, MultipleTeamDrive) {
  const std::vector<std::pair<std::string, std::string>> team_drives = {
      {"the1stTeamDriveId", "The First Team Drive"},
      {"the2ndTeamDriveId", "The Second Team Drive"},
      {"the3rdTeamDriveId", "The Third Team Drive"},
      {"the4thTeamDriveId", "The Forth Team Drive"},
  };

  for (const auto& drive : team_drives) {
    drive_service_->AddTeamDrive(drive.first, drive.second);
  }

  FileError error = FILE_ERROR_FAILED;
  team_drive_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1, drive_service_->team_drive_list_load_count());

  EXPECT_EQ(team_drives.size(),
            team_drive_list_observer_->team_drives_list().size());
  EXPECT_EQ(team_drives.size(),
            team_drive_list_observer_->added_team_drives().size());
  EXPECT_TRUE(team_drive_list_observer_->removed_team_drives().empty());

  ResourceEntryVector entries;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->ReadDirectoryByPath(
                               util::GetDriveTeamDrivesRootPath(), &entries));
  EXPECT_EQ(team_drives.size(), entries.size());

  ResourceEntry entry;

  for (const auto& drive : team_drives) {
    EXPECT_EQ(FILE_ERROR_OK,
              metadata_->GetResourceEntryByPath(
                  util::GetDriveTeamDrivesRootPath().AppendASCII(drive.second),
                  &entry));
    EXPECT_EQ(drive.first, entry.resource_id());
    EXPECT_EQ(drive.second, entry.base_name());
    EXPECT_TRUE(entry.file_info().is_directory());
    EXPECT_EQ("", entry.directory_specific_info().start_page_token());
  }
}

// Tests that is we have existing team drives in local metadata, and we add
// new team drives from the server, that we retain the pre-existing local
// metadata.
TEST_F(TeamDriveListLoaderTest, RetainExistingMetadata) {
  const std::vector<std::pair<std::string, std::string>> new_team_drives = {
      {"the3rdTeamDriveId", "The Third Team Drive"},
      {"the4thTeamDriveId", "The Forth Team Drive"},
  };

  const std::vector<std::pair<std::string, std::string>> old_team_drives = {
      {"the1stTeamDriveId", "The First Team Drive"},
      {"the2ndTeamDriveId", "The Second Team Drive"},
  };

  for (const auto& drive : old_team_drives) {
    drive_service_->AddTeamDrive(drive.first, drive.second);
  }

  for (const auto& drive : new_team_drives) {
    drive_service_->AddTeamDrive(drive.first, drive.second);
  }

  std::string root_local_id;
  ASSERT_EQ(FILE_ERROR_OK,
            metadata_->GetIdByPath(util::GetDriveTeamDrivesRootPath(),
                                   &root_local_id));

  for (const auto& drive : old_team_drives) {
    std::string local_id;
    ASSERT_EQ(FILE_ERROR_OK,
              metadata_->AddEntry(CreateDirectoryEntryWithResourceId(
                                      drive.second, drive.first, root_local_id),
                                  &local_id));
  }

  FileError error = FILE_ERROR_FAILED;
  team_drive_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1, drive_service_->team_drive_list_load_count());

  EXPECT_EQ(old_team_drives.size() + new_team_drives.size(),
            team_drive_list_observer_->team_drives_list().size());
  EXPECT_EQ(new_team_drives.size(),
            team_drive_list_observer_->added_team_drives().size());
  EXPECT_TRUE(team_drive_list_observer_->removed_team_drives().empty());

  ResourceEntryVector entries;
  EXPECT_EQ(FILE_ERROR_OK, metadata_->ReadDirectoryByPath(
                               util::GetDriveTeamDrivesRootPath(), &entries));
  EXPECT_EQ(new_team_drives.size() + old_team_drives.size(), entries.size());

  ResourceEntry entry;
  for (const auto& drive : new_team_drives) {
    EXPECT_EQ(FILE_ERROR_OK,
              metadata_->GetResourceEntryByPath(
                  util::GetDriveTeamDrivesRootPath().AppendASCII(drive.second),
                  &entry));
    EXPECT_EQ(drive.first, entry.resource_id());
    EXPECT_EQ(drive.second, entry.base_name());
    EXPECT_TRUE(entry.file_info().is_directory());
    EXPECT_EQ("", entry.directory_specific_info().start_page_token());
  }

  for (const auto& drive : old_team_drives) {
    EXPECT_EQ(FILE_ERROR_OK,
              metadata_->GetResourceEntryByPath(
                  util::GetDriveTeamDrivesRootPath().AppendASCII(drive.second),
                  &entry));
    EXPECT_EQ(drive.first, entry.resource_id());
    EXPECT_EQ(drive.second, entry.base_name());
    EXPECT_TRUE(entry.file_info().is_directory());
    EXPECT_EQ(kTestStartPageToken,
              entry.directory_specific_info().start_page_token());
  }
}

// Tests that if we we had team drives stored locally that are no longer on the
// server that we deleted them correctly.
TEST_F(TeamDriveListLoaderTest, RemoveMissingTeamDriveFromMetadata) {
  const std::vector<std::pair<std::string, std::string>> old_team_drives = {
      {"the1stTeamDriveId", "The First Team Drive"},
      {"the2ndTeamDriveId", "The Second Team Drive"},
  };

  std::string root_local_id;
  ASSERT_EQ(FILE_ERROR_OK,
            metadata_->GetIdByPath(util::GetDriveTeamDrivesRootPath(),
                                   &root_local_id));

  for (const auto& drive : old_team_drives) {
    std::string local_id;
    ASSERT_EQ(FILE_ERROR_OK,
              metadata_->AddEntry(CreateDirectoryEntryWithResourceId(
                                      drive.second, drive.first, root_local_id),
                                  &local_id));
  }

  FileError error = FILE_ERROR_FAILED;
  team_drive_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(&error));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  EXPECT_EQ(1, drive_service_->team_drive_list_load_count());

  EXPECT_TRUE(team_drive_list_observer_->team_drives_list().empty());
  EXPECT_TRUE(team_drive_list_observer_->added_team_drives().empty());
  EXPECT_EQ(old_team_drives.size(),
            team_drive_list_observer_->removed_team_drives().size());

  ResourceEntry entry;
  for (const auto& drive : old_team_drives) {
    EXPECT_EQ(FILE_ERROR_NOT_FOUND,
              metadata_->GetResourceEntryByPath(
                  util::GetDriveTeamDrivesRootPath().AppendASCII(drive.second),
                  &entry));
  }
}

}  // namespace internal
}  // namespace drive
