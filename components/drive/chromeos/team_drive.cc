// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/chromeos/team_drive.h"

namespace drive {
namespace internal {

TeamDrive::TeamDrive(const std::string& team_drive_id)
    : team_drive_id_(team_drive_id) {}

TeamDrive::TeamDrive(const std::string& team_drive_id,
                     const std::string& team_drive_name,
                     const base::FilePath& team_drive_path)
    : team_drive_id_(team_drive_id),
      team_drive_name_(team_drive_name),
      team_drive_path_(team_drive_path) {}

TeamDrive::~TeamDrive() = default;

}  // namespace internal
}  // namespace drive
