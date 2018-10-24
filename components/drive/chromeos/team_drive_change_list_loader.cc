// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/team_drive_change_list_loader.h"

#include <memory>

#include "base/sequenced_task_runner.h"
#include "components/drive/chromeos/change_list_loader.h"
#include "components/drive/chromeos/directory_loader.h"
#include "components/drive/chromeos/root_folder_id_loader.h"
#include "components/drive/chromeos/start_page_token_loader.h"

namespace drive {
namespace internal {

namespace {

class ConstantRootFolderIdLoader : public RootFolderIdLoader {
 public:
  explicit ConstantRootFolderIdLoader(const std::string& team_drive_id)
      : team_drive_id_(team_drive_id) {}
  ~ConstantRootFolderIdLoader() override = default;

  void GetRootFolderId(const RootFolderIdCallback& callback) override {
    callback.Run(FILE_ERROR_OK, team_drive_id_);
  }

 private:
  const std::string team_drive_id_;
};

}  // namespace

TeamDriveChangeListLoader::TeamDriveChangeListLoader(
    const std::string& team_drive_id,
    const base::FilePath& root_entry_path,
    EventLogger* logger,
    base::SequencedTaskRunner* blocking_task_runner,
    ResourceMetadata* resource_metadata,
    JobScheduler* scheduler,
    LoaderController* apply_task_controller)
    : team_drive_id_(team_drive_id), root_entry_path_(root_entry_path) {
  root_folder_id_loader_ =
      std::make_unique<ConstantRootFolderIdLoader>(team_drive_id_);

  start_page_token_loader_ =
      std::make_unique<StartPageTokenLoader>(team_drive_id_, scheduler);

  change_list_loader_ = std::make_unique<ChangeListLoader>(
      logger, blocking_task_runner, resource_metadata, scheduler,
      root_folder_id_loader_.get(), start_page_token_loader_.get(),
      apply_task_controller, team_drive_id_, root_entry_path_);
  change_list_loader_->AddObserver(this);

  directory_loader_ = std::make_unique<DirectoryLoader>(
      logger, blocking_task_runner, resource_metadata, scheduler,
      root_folder_id_loader_.get(), start_page_token_loader_.get(),
      apply_task_controller, root_entry_path_);
  directory_loader_->AddObserver(this);
}

TeamDriveChangeListLoader::~TeamDriveChangeListLoader() = default;

// DriveChangeListLoader overrides
void TeamDriveChangeListLoader::AddChangeListLoaderObserver(
    ChangeListLoaderObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  change_list_loader_observers_.AddObserver(observer);
}

void TeamDriveChangeListLoader::RemoveChangeListLoaderObserver(
    ChangeListLoaderObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  change_list_loader_observers_.RemoveObserver(observer);
}

void TeamDriveChangeListLoader::AddTeamDriveListObserver(
    TeamDriveListObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void TeamDriveChangeListLoader::RemoveTeamDriveListObserver(
    TeamDriveListObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool TeamDriveChangeListLoader::IsRefreshing() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return change_list_loader_->IsRefreshing();
}

void TeamDriveChangeListLoader::LoadIfNeeded(
    const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  change_list_loader_->LoadIfNeeded(callback);
}

void TeamDriveChangeListLoader::ReadDirectory(
    const base::FilePath& directory_path,
    const ReadDirectoryEntriesCallback& entries_callback,
    const FileOperationCallback& completion_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(root_entry_path_ == directory_path ||
         root_entry_path_.IsParent(directory_path))
      << "Directory paths are not related: " << root_entry_path_.value()
      << " -> " << directory_path.value();

  directory_loader_->ReadDirectory(directory_path, entries_callback,
                                   completion_callback);
}

void TeamDriveChangeListLoader::CheckForUpdates(
    const FileOperationCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  change_list_loader_->CheckForUpdates(callback);
}

void TeamDriveChangeListLoader::OnDirectoryReloaded(
    const base::FilePath& directory_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : change_list_loader_observers_) {
    observer.OnDirectoryReloaded(directory_path);
  }
}

void TeamDriveChangeListLoader::OnFileChanged(const FileChange& changed_files) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : change_list_loader_observers_) {
    observer.OnFileChanged(changed_files);
  }
}

void TeamDriveChangeListLoader::OnLoadFromServerComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : change_list_loader_observers_) {
    observer.OnLoadFromServerComplete();
  }
}

void TeamDriveChangeListLoader::OnInitialLoadComplete() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : change_list_loader_observers_) {
    observer.OnInitialLoadComplete();
  }
}

}  // namespace internal
}  // namespace drive
