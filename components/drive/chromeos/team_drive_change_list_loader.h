// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_CHANGE_LIST_LOADER_H_
#define COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_CHANGE_LIST_LOADER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "components/drive/chromeos/change_list_loader_observer.h"
#include "components/drive/chromeos/drive_change_list_loader.h"

namespace drive {

class EventLogger;
class JobScheduler;

namespace internal {

class ChangeListLoader;
class DirectoryLoader;
class LoaderController;
class RootFolderIdLoader;
class StartPageTokenLoader;

// TeamDriveChangeListLoader loads change lists for a specific team drive id.
// It uses directory loader and change list loader to retrieve change lists
// into resouce metadata. There should be one TeamDriveChangeListLoader created
// for every team drive that the user has access to.
class TeamDriveChangeListLoader : public DriveChangeListLoader,
                                  public ChangeListLoaderObserver {
 public:
  TeamDriveChangeListLoader(const std::string& team_drive_id,
                            const base::FilePath& root_entry_path,
                            EventLogger* logger,
                            base::SequencedTaskRunner* blocking_task_runner,
                            ResourceMetadata* resource_metadata,
                            JobScheduler* scheduler,
                            LoaderController* apply_task_controller);

  ~TeamDriveChangeListLoader() override;

  const base::FilePath& root_entry_path() const { return root_entry_path_; }

  // DriveChangeListLoader overrides
  void AddChangeListLoaderObserver(ChangeListLoaderObserver* observer) override;
  void RemoveChangeListLoaderObserver(
      ChangeListLoaderObserver* observer) override;
  void AddTeamDriveListObserver(TeamDriveListObserver* observer) override;
  void RemoveTeamDriveListObserver(TeamDriveListObserver* observer) override;
  bool IsRefreshing() override;
  void LoadIfNeeded(const FileOperationCallback& callback) override;
  void ReadDirectory(const base::FilePath& directory_path,
                     const ReadDirectoryEntriesCallback& entries_callback,
                     const FileOperationCallback& completion_callback) override;
  void CheckForUpdates(const FileOperationCallback& callback) override;

  // ChangeListLoaderObserver overrides
  void OnDirectoryReloaded(const base::FilePath& directory_path) override;
  void OnFileChanged(const FileChange& changed_files) override;
  void OnLoadFromServerComplete() override;
  void OnInitialLoadComplete() override;

 private:
  std::unique_ptr<RootFolderIdLoader> root_folder_id_loader_;
  std::unique_ptr<StartPageTokenLoader> start_page_token_loader_;
  std::unique_ptr<ChangeListLoader> change_list_loader_;
  std::unique_ptr<DirectoryLoader> directory_loader_;

  const std::string team_drive_id_;
  const base::FilePath root_entry_path_;
  base::ObserverList<ChangeListLoaderObserver> change_list_loader_observers_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(TeamDriveChangeListLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_CHANGE_LIST_LOADER_H_
