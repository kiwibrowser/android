// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/drive/chromeos/team_drive_change_list_loader.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/drive/chromeos/drive_file_util.h"
#include "components/drive/chromeos/drive_test_util.h"
#include "components/drive/chromeos/file_cache.h"
#include "components/drive/chromeos/loader_controller.h"
#include "components/drive/chromeos/resource_metadata.h"
#include "components/drive/event_logger.h"
#include "components/drive/file_change.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "components/drive/resource_metadata_storage.h"
#include "components/drive/service/fake_drive_service.h"
#include "components/drive/service/test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

constexpr char kTeamDriveId[] = "team_drive_id";
constexpr char kTeamDriveName[] = "team_drive_name";
constexpr char kTestStartPageToken[] = "1";

class TestChangeListLoaderObserver : public ChangeListLoaderObserver {
 public:
  TestChangeListLoaderObserver()
      : load_from_server_complete_count_(0), initial_load_complete_count_(0) {}

  ~TestChangeListLoaderObserver() override = default;

  const FileChange& changed_files() const { return changed_files_; }
  void clear_changed_files() { changed_files_.ClearForTest(); }

  int load_from_server_complete_count() const {
    return load_from_server_complete_count_;
  }
  int initial_load_complete_count() const {
    return initial_load_complete_count_;
  }

  // ChageListObserver overrides:
  void OnFileChanged(const FileChange& changed_files) override {
    changed_files_.Apply(changed_files);
  }
  void OnLoadFromServerComplete() override {
    ++load_from_server_complete_count_;
  }
  void OnInitialLoadComplete() override { ++initial_load_complete_count_; }

 private:
  FileChange changed_files_;
  int load_from_server_complete_count_;
  int initial_load_complete_count_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeListLoaderObserver);
};

}  // namespace

class TeamDriveChangeListLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    test_util::RegisterDrivePrefs(pref_service_->registry());

    logger_ = std::make_unique<EventLogger>();

    drive_service_ = std::make_unique<FakeDriveService>();
    ASSERT_TRUE(test_util::SetUpTeamDriveTestEntries(
        drive_service_.get(), kTeamDriveId, kTeamDriveName));

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
    base::FilePath root_entry_path =
        util::GetDriveTeamDrivesRootPath().Append(kTeamDriveName);

    team_drive_change_list_loader_ =
        std::make_unique<TeamDriveChangeListLoader>(
            kTeamDriveId, root_entry_path, logger_.get(),
            base::ThreadTaskRunnerHandle::Get().get(), metadata_.get(),
            scheduler_.get(), loader_controller_.get());

    change_list_loader_observer_ =
        std::make_unique<TestChangeListLoaderObserver>();
    team_drive_change_list_loader_->AddChangeListLoaderObserver(
        change_list_loader_observer_.get());
  }

  // Adds a team drive entry to ResourceMetadata
  void AddTeamDriveEntry(const std::string& id,
                         const std::string& name,
                         const std::string& start_page_token) {
    std::string root_local_id;
    ASSERT_EQ(FILE_ERROR_OK,
              metadata_->GetIdByPath(util::GetDriveTeamDrivesRootPath(),
                                     &root_local_id));

    std::string local_id;
    ASSERT_EQ(FILE_ERROR_OK, metadata_->AddEntry(
                                 CreateDirectoryEntryWithResourceId(
                                     name, id, root_local_id, start_page_token),
                                 &local_id));
  }

  // Creates a ResourceEntry for a directory with explicitly set resource_id.
  ResourceEntry CreateDirectoryEntryWithResourceId(
      const std::string& title,
      const std::string& resource_id,
      const std::string& parent_local_id,
      const std::string& start_page_token) {
    ResourceEntry entry;
    entry.set_title(title);
    entry.set_resource_id(resource_id);
    entry.set_parent_local_id(parent_local_id);
    entry.mutable_file_info()->set_is_directory(true);
    entry.mutable_directory_specific_info()->set_start_page_token(
        start_page_token);
    return entry;
  }

  // Adds a new file to the root directory of the service.
  std::unique_ptr<google_apis::FileResource> AddNewFile(
      const std::string& title,
      const std::string& parent_resource_id) {
    google_apis::DriveApiErrorCode error = google_apis::DRIVE_FILE_ERROR;
    std::unique_ptr<google_apis::FileResource> entry;
    drive_service_->AddNewFile(
        "text/plain", "content text", parent_resource_id, title,
        false,  // shared_with_me
        google_apis::test_util::CreateCopyResultCallback(&error, &entry));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(google_apis::HTTP_CREATED, error);
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
  std::unique_ptr<TeamDriveChangeListLoader> team_drive_change_list_loader_;
  std::unique_ptr<TestChangeListLoaderObserver> change_list_loader_observer_;
};

// When loading, if the local resource metadata contains a start page token then
// we do not load from the server, just use local metadata.
TEST_F(TeamDriveChangeListLoaderTest, MetadataRefresh) {
  AddTeamDriveEntry(kTeamDriveId, kTeamDriveName, kTestStartPageToken);
  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());

  FileError error = FILE_ERROR_FAILED;
  team_drive_change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  EXPECT_EQ(1, drive_service_->start_page_token_load_count());
  EXPECT_EQ(0, drive_service_->change_list_load_count());
}

// Trigger a full load, by setting the start page token of the team drive in
// resource metadata to an empty string.
TEST_F(TeamDriveChangeListLoaderTest, FullLoad) {
  AddTeamDriveEntry(kTeamDriveId, kTeamDriveName, std::string());
  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());

  FileError error = FILE_ERROR_FAILED;
  team_drive_change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_TRUE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);

  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  EXPECT_EQ(1, drive_service_->start_page_token_load_count());
  EXPECT_EQ(1, drive_service_->file_list_load_count());
  EXPECT_EQ(0, drive_service_->change_list_load_count());
  EXPECT_EQ(1, change_list_loader_observer_->initial_load_complete_count());
  EXPECT_EQ(1, change_list_loader_observer_->load_from_server_complete_count());
  EXPECT_TRUE(change_list_loader_observer_->changed_files().empty());

  base::FilePath file_path = util::GetDriveTeamDrivesRootPath()
                                 .AppendASCII(kTeamDriveName)
                                 .AppendASCII("File 1.txt");
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(file_path, &entry));
}

// CheckForUpdates will trigger a full and delta load from the server
TEST_F(TeamDriveChangeListLoaderTest, CheckForUpdates) {
  AddTeamDriveEntry(kTeamDriveId, kTeamDriveName, std::string());

  // CheckForUpdates() results in no-op before load.
  FileError error = FILE_ERROR_FAILED;
  team_drive_change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(&error));
  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_FAILED, error);

  EXPECT_EQ(0, drive_service_->file_list_load_count());
  EXPECT_EQ(0, drive_service_->start_page_token_load_count());

  // Start initial load.
  FileError load_error = FILE_ERROR_FAILED;
  team_drive_change_list_loader_->LoadIfNeeded(
      google_apis::test_util::CreateCopyResultCallback(&load_error));
  EXPECT_TRUE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();

  std::string start_page_token;
  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  EXPECT_EQ(FILE_ERROR_OK, load_error);
  EXPECT_EQ(FILE_ERROR_OK,
            internal::GetStartPageToken(metadata_.get(), kTeamDriveId,
                                        &start_page_token));
  EXPECT_FALSE(start_page_token.empty());
  EXPECT_EQ(1, drive_service_->file_list_load_count());

  std::string previous_start_page_token = start_page_token;
  // CheckForUpdates() results in no update.
  FileError check_for_updates_error = FILE_ERROR_FAILED;
  team_drive_change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  EXPECT_EQ(FILE_ERROR_OK,
            internal::GetStartPageToken(metadata_.get(), kTeamDriveId,
                                        &start_page_token));
  EXPECT_EQ(previous_start_page_token, start_page_token);

  // Add a file to the service.
  std::unique_ptr<google_apis::FileResource> gdata_entry =
      AddNewFile("New File", kTeamDriveId);
  ASSERT_TRUE(gdata_entry);

  // CheckForUpdates() results in update.
  change_list_loader_observer_->clear_changed_files();
  team_drive_change_list_loader_->CheckForUpdates(
      google_apis::test_util::CreateCopyResultCallback(
          &check_for_updates_error));
  EXPECT_TRUE(team_drive_change_list_loader_->IsRefreshing());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(team_drive_change_list_loader_->IsRefreshing());
  EXPECT_EQ(FILE_ERROR_OK,
            internal::GetStartPageToken(metadata_.get(), kTeamDriveId,
                                        &start_page_token));
  EXPECT_NE(previous_start_page_token, start_page_token);
  EXPECT_EQ(2, change_list_loader_observer_->load_from_server_complete_count());

  base::FilePath team_drive_path =
      util::GetDriveTeamDrivesRootPath().AppendASCII(kTeamDriveName);
  EXPECT_TRUE(change_list_loader_observer_->changed_files().CountDirectory(
      team_drive_path));

  // The new file is found in the local metadata.
  base::FilePath new_file_path =
      team_drive_path.AppendASCII(gdata_entry->title());
  ResourceEntry entry;
  EXPECT_EQ(FILE_ERROR_OK,
            metadata_->GetResourceEntryByPath(new_file_path, &entry));
}

}  // namespace internal
}  // namespace drive
