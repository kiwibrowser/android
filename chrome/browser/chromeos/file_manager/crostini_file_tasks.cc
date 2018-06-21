// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/crostini_file_tasks.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "ui/base/layout.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

void FindCrostiniTasks(Profile* profile,
                       const std::vector<extensions::EntryInfo>& entries,
                       std::vector<FullTaskDescriptor>* result_list,
                       base::OnceClosure completion_closure) {
  if (!IsCrostiniUIAllowedForProfile(profile)) {
    std::move(completion_closure).Run();
    return;
  }

  // We currently don't support opening directories or files not already inside
  // the Crostini directories.
  base::FilePath crostini_mount = util::GetCrostiniMountDirectory(profile);
  for (const extensions::EntryInfo& entry : entries) {
    if (!crostini_mount.IsParent(entry.path) ||
        entry.path.EndsWithSeparator()) {
      std::move(completion_closure).Run();
      return;
    }
  }

  std::set<std::string> target_mime_types;
  for (const extensions::EntryInfo& entry : entries)
    target_mime_types.insert(entry.mime_type);

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile);
  for (const std::string& app_id : registry_service->GetRegisteredAppIds()) {
    crostini::CrostiniRegistryService::Registration registration =
        *registry_service->GetRegistration(app_id);

    const std::set<std::string>& supported_mime_types =
        registration.MimeTypes();
    bool had_unsupported_mime_type = false;
    for (const std::string& target_mime_type : target_mime_types) {
      if (supported_mime_types.find(target_mime_type) !=
          supported_mime_types.end())
        continue;
      had_unsupported_mime_type = true;
      break;
    }
    if (had_unsupported_mime_type)
      continue;

    // TODO(timloh): Add support for Crostini icons
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(app_id, TASK_TYPE_CROSTINI_APP, kCrostiniAppActionID),
        registration.Name(),
        extensions::api::file_manager_private::Verb::VERB_OPEN_WITH, GURL(),
        false /* is_default */, false /* is_generic */));
  }

  std::move(completion_closure).Run();
}

void ExecuteCrostiniTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    const FileTaskFinishedCallback& done) {
  DCHECK(IsCrostiniUIAllowedForProfile(profile));

  base::FilePath folder(util::GetCrostiniMountPointName(profile));

  std::vector<std::string> files;
  for (const storage::FileSystemURL& file_system_url : file_system_urls) {
    DCHECK(file_system_url.mount_type() == storage::kFileSystemTypeExternal);
    DCHECK(file_system_url.type() == storage::kFileSystemTypeNativeLocal);

    // Reformat virtual_path()
    // from <mount_label>/path/to/file
    // to   /<home-directory>/path/to/file
    base::FilePath result = HomeDirectoryForProfile(profile);
    bool success =
        folder.AppendRelativePath(file_system_url.virtual_path(), &result);
    DCHECK(success);
    files.emplace_back(result.AsUTF8Unsafe());
  }

  LaunchCrostiniApp(profile, task.app_id, display::kInvalidDisplayId, files);
}

}  // namespace file_tasks
}  // namespace file_manager
