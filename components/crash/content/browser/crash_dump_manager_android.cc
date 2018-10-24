// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/crash_dump_manager_android.h"

#include <stdint.h>

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#include "jni/CrashDumpManager_jni.h"

namespace breakpad {

namespace {

base::LazyInstance<CrashDumpManager>::Leaky g_instance =
    LAZY_INSTANCE_INITIALIZER;

class DefaultUploader : public CrashDumpManager::Uploader {
 public:
  DefaultUploader() = default;
  ~DefaultUploader() override = default;

  // CrashDumpManager::Uploader:
  void TryToUploadCrashDump(const base::FilePath& crash_dump_path) override {
    // Hop over to Java to attempt to attach the logcat to the crash. This may
    // fail, which is ok -- if it does, the crash will still be uploaded on the
    // next browser start.
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jstring> j_dump_path =
        base::android::ConvertUTF8ToJavaString(env, crash_dump_path.value());
    Java_CrashDumpManager_tryToUploadMinidump(env, j_dump_path);
  }
};

}  // namespace

// static
CrashDumpManager* CrashDumpManager::GetInstance() {
  return g_instance.Pointer();
}

CrashDumpManager::CrashDumpManager()
    : uploader_(std::make_unique<DefaultUploader>()) {}

CrashDumpManager::~CrashDumpManager() {}

base::ScopedFD CrashDumpManager::CreateMinidumpFileForChild(
    int process_host_id) {
  base::AssertBlockingAllowed();
  base::FilePath minidump_path;
  if (!base::CreateTemporaryFile(&minidump_path)) {
    LOG(ERROR) << "Failed to create temporary file, crash won't be reported.";
    return base::ScopedFD();
  }

  // We need read permission as the minidump is generated in several phases
  // and needs to be read at some point.
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_WRITE;
  base::File minidump_file(minidump_path, flags);
  if (!minidump_file.IsValid()) {
    LOG(ERROR) << "Failed to open temporary file, crash won't be reported.";
    return base::ScopedFD();
  }

  SetMinidumpPath(process_host_id, minidump_path);
  return base::ScopedFD(minidump_file.TakePlatformFile());
}

void CrashDumpManager::ProcessMinidumpFileFromChild(
    base::FilePath crash_dump_dir,
    const CrashDumpObserver::TerminationInfo& info) {
  CrashDumpManager::CrashDumpStatus status =
      ProcessMinidumpFileFromChildInternal(crash_dump_dir, info);
  crash_reporter::CrashMetricsReporter::GetInstance()->CrashDumpProcessed(
      info, status);
}

CrashDumpManager::CrashDumpStatus
CrashDumpManager::ProcessMinidumpFileFromChildInternal(
    base::FilePath crash_dump_dir,
    const CrashDumpObserver::TerminationInfo& info) {
  base::AssertBlockingAllowed();
  base::FilePath minidump_path;
  // If the minidump for a given child process has already been
  // processed, then there is no more work to do.
  if (!GetMinidumpPath(info.process_host_id, &minidump_path)) {
    return CrashDumpStatus::kMissingDump;
  }

  int64_t file_size = 0;

  if (!base::PathExists(minidump_path)) {
    LOG(ERROR) << "minidump does not exist " << minidump_path.value();
    return CrashDumpStatus::kMissingDump;
  }

  int r = base::GetFileSize(minidump_path, &file_size);
  DCHECK(r) << "Failed to retrieve size for minidump " << minidump_path.value();

  if (file_size == 0) {
    // Empty minidump, this process did not crash. Just remove the file.
    r = base::DeleteFile(minidump_path, false);
    DCHECK(r) << "Failed to delete temporary minidump file "
              << minidump_path.value();
    return CrashDumpStatus::kEmptyDump;
  }

  // We are dealing with a valid minidump. Copy it to the crash report
  // directory from where Java code will upload it later on.
  if (crash_dump_dir.empty()) {
    NOTREACHED() << "Failed to retrieve the crash dump directory.";
    return CrashDumpStatus::kDumpProcessingFailed;
  }
  const uint64_t rand = base::RandUint64();
  const std::string filename =
      base::StringPrintf("chromium-renderer-minidump-%016" PRIx64 ".dmp%d",
                         rand, info.process_host_id);
  base::FilePath dest_path = crash_dump_dir.Append(filename);
  r = base::Move(minidump_path, dest_path);
  if (!r) {
    LOG(ERROR) << "Failed to move crash dump from " << minidump_path.value()
               << " to " << dest_path.value();
    base::DeleteFile(minidump_path, false);
    return CrashDumpStatus::kDumpProcessingFailed;
  }
  VLOG(1) << "Crash minidump successfully generated: " << dest_path.value();

  uploader_->TryToUploadCrashDump(dest_path);
  return CrashDumpStatus::kValidDump;
}

void CrashDumpManager::SetMinidumpPath(int process_host_id,
                                       const base::FilePath& minidump_path) {
  base::AutoLock auto_lock(process_host_id_to_minidump_path_lock_);
  DCHECK(
      !base::ContainsKey(process_host_id_to_minidump_path_, process_host_id));
  process_host_id_to_minidump_path_[process_host_id] = minidump_path;
}

bool CrashDumpManager::GetMinidumpPath(int process_host_id,
                                       base::FilePath* minidump_path) {
  base::AutoLock auto_lock(process_host_id_to_minidump_path_lock_);
  ChildProcessIDToMinidumpPath::iterator iter =
      process_host_id_to_minidump_path_.find(process_host_id);
  if (iter == process_host_id_to_minidump_path_.end()) {
    return false;
  }
  *minidump_path = iter->second;
  process_host_id_to_minidump_path_.erase(iter);
  return true;
}

}  // namespace breakpad
