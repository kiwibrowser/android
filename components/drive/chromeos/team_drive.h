// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_H_
#define COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"

namespace drive {
namespace internal {

// Represents a team drive object that can be passed to observers of team drive
// changes, for example when loading the list of team drives, or removing/adding
// a team drive when processing the users change list.
class TeamDrive {
 public:
  TeamDrive(const std::string& team_drive_id);

  TeamDrive(const std::string& team_drive_id,
            const std::string& team_drive_name,
            const base::FilePath& team_drive_path);
  ~TeamDrive();

  // The team drive ID returned from the server.
  // https://developers.google.com/drive/api/v2/reference/teamdrives#resource
  const std::string& team_drive_id() const { return team_drive_id_; }

  // The team drive name returned from the server. May be empty when deleting
  // a team drive, which will be based on the id() and not the name.
  const std::string& team_drive_name() const { return team_drive_name_; }

  // The path where the team drive is mounted in the file system. May be
  // empty when deleting the team drive.
  const base::FilePath& team_drive_path() const { return team_drive_path_; }

 private:
  std::string team_drive_id_;
  std::string team_drive_name_;
  base::FilePath team_drive_path_;
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_TEAM_DRIVE_H_
