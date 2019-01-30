// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_LIST_OBSERVER_H_
#define COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_LIST_OBSERVER_H_

#include <vector>

#include "components/drive/chromeos/team_drive.h"

namespace drive {
namespace internal {

// Interface for classes that need to observe events from the
// TeamDriveListLoader.
// TODO(slangley): Consider merging with ChangeListLoaderObserver.
class TeamDriveListObserver {
 public:
  // Called every time that the complete list of team drives has been retrieved
  // from the server.
  // - |team_drives_list| is the entire list of team drives the user has
  //   access to.
  // - |added_team_drives| is the list of team drives that have been added
  //   as part of loading the team drives.
  // - |removed_team_drives| will be the list of team drives that have been
  //   removed as part of loading the team drives.
  virtual void OnTeamDriveListLoaded(
      const std::vector<TeamDrive>& team_drives_list,
      const std::vector<TeamDrive>& added_team_drives,
      const std::vector<TeamDrive>& removed_team_drives) = 0;

 protected:
  virtual ~TeamDriveListObserver() = default;
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_LIST_LOADER_OBSERVER_H_
