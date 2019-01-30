// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_

#include <map>
#include <memory>
#include <utility>

#include "base/android/application_status_listener.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/crash/content/browser/crash_dump_observer_android.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/process_type.h"

namespace breakpad {

// This class manages the crash minidumps.
// On Android, because of process isolation, each renderer process runs with a
// different UID. As a result, we cannot generate the minidumps in the browser
// (as the browser process does not have access to some system files for the
// crashed process). So the minidump is generated in the renderer process.
// Since the isolated process cannot open files, we provide it on creation with
// a file descriptor into which to write the minidump in the event of a crash.
// This class creates these file descriptors and associates them with render
// processes and takes the appropriate action when the render process
// terminates.
class CrashDumpManager {
 public:
  enum class CrashDumpStatus {
    // The dump for this process did not have a path set. This can happen if the
    // dump was already processed or if crash dump generation is not turned on.
    kMissingDump,

    // The crash dump was empty.
    kEmptyDump,

    // Minidump file was found, but could not be copied to crash dir.
    kDumpProcessingFailed,

    // The crash dump is valid.
    kValidDump,
  };

  // Class which aids in uploading a crash dump.
  class Uploader {
   public:
    virtual ~Uploader() = default;

    // Attempts to upload the specified child process minidump.
    virtual void TryToUploadCrashDump(
        const base::FilePath& crash_dump_path) = 0;
  };

  static CrashDumpManager* GetInstance();

  void ProcessMinidumpFileFromChild(
      base::FilePath crash_dump_dir,
      const CrashDumpObserver::TerminationInfo& info);

  base::ScopedFD CreateMinidumpFileForChild(int process_host_id);

  // Careful, |uploader_| is accessed on one of the task scheduler threads.
  // Tests should set this before any other threads are spawned.
  void set_uploader_for_testing(std::unique_ptr<Uploader> uploader) {
    DCHECK(uploader);
    uploader_ = std::move(uploader);
  }

 private:
  friend struct base::LazyInstanceTraitsBase<CrashDumpManager>;

  CrashDumpManager();
  ~CrashDumpManager();

  CrashDumpStatus ProcessMinidumpFileFromChildInternal(
      base::FilePath crash_dump_dir,
      const CrashDumpObserver::TerminationInfo& info);

  typedef std::map<int, base::FilePath> ChildProcessIDToMinidumpPath;

  void SetMinidumpPath(int process_host_id,
                       const base::FilePath& minidump_path);
  bool GetMinidumpPath(int process_host_id, base::FilePath* minidump_path);

  // Should never be nullptr.
  std::unique_ptr<Uploader> uploader_;

  // This map should only be accessed with its lock aquired as it is accessed
  // from the PROCESS_LAUNCHER and UI threads.
  base::Lock process_host_id_to_minidump_path_lock_;
  ChildProcessIDToMinidumpPath process_host_id_to_minidump_path_;

  DISALLOW_COPY_AND_ASSIGN(CrashDumpManager);
};

}  // namespace breakpad

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_CRASH_DUMP_MANAGER_ANDROID_H_
