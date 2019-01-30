// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/win/windows_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_elf/chrome_elf_main.h"
#include "chrome_elf/sha1/sha1.h"
#include "chrome_elf/third_party_dlls/logging_api.h"

// This function is a temporary workaround for https://crbug.com/655788. We
// need to come up with a better way to initialize crash reporting that can
// happen inside DllMain().
void SignalInitializeCrashReporting() {}

void SignalChromeElf() {}

bool GetUserDataDirectoryThunk(wchar_t* user_data_dir,
                               size_t user_data_dir_length,
                               wchar_t* invalid_user_data_dir,
                               size_t invalid_user_data_dir_length) {
  // In tests, just respect the user-data-dir switch if given.
  base::FilePath user_data_dir_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kUserDataDir);
  if (!user_data_dir_path.empty() && user_data_dir_path.EndsWithSeparator())
    user_data_dir_path = user_data_dir_path.StripTrailingSeparators();

  wcsncpy_s(user_data_dir, user_data_dir_length,
            user_data_dir_path.value().c_str(), _TRUNCATE);
  wcsncpy_s(invalid_user_data_dir, invalid_user_data_dir_length, L"",
            _TRUNCATE);

  return !user_data_dir_path.empty();
}

void SetMetricsClientId(const char* client_id) {}

struct TestLogEntry {
  third_party_dlls::LogType log_type;
  uint8_t basename_hash[elf_sha1::kSHA1Length];
  uint8_t code_id_hash[elf_sha1::kSHA1Length];
};

// This test stub always writes 2 hardcoded entries into the buffer, if the
// buffer size is large enough.
uint32_t DrainLog(uint8_t* buffer,
                  uint32_t buffer_size,
                  uint32_t* log_remaining) {
  // Alternate between log types.
  TestLogEntry kTestLogEntries[] = {
      {
          third_party_dlls::LogType::kAllowed,
          {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
           0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19},
          {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
           0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49},
      },
      {
          third_party_dlls::LogType::kBlocked,
          {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
           0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB},
          {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
           0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD},
      },
  };

  // Each entry shares the module path for convenience.
  static constexpr char kModulePath[] = "C:\\foo\\bar\\module.dll";
  static constexpr uint32_t kModulePathLength = base::size(kModulePath) - 1;

  if (log_remaining) {
    *log_remaining = third_party_dlls::GetLogEntrySize(kModulePathLength) *
                     base::size(kTestLogEntries);
  }

  uint8_t* tracker = buffer;
  for (const auto& test_entry : kTestLogEntries) {
    uint32_t entry_size = third_party_dlls::GetLogEntrySize(kModulePathLength);
    if (tracker + entry_size > buffer + buffer_size)
      break;

    third_party_dlls::LogEntry* log_entry =
        reinterpret_cast<third_party_dlls::LogEntry*>(tracker);

    log_entry->type = test_entry.log_type;
    ::memcpy(log_entry->basename_hash, test_entry.basename_hash,
             sizeof(test_entry.basename_hash));
    ::memcpy(log_entry->code_id_hash, test_entry.code_id_hash,
             sizeof(test_entry.code_id_hash));
    log_entry->path_len = kModulePathLength;
    ::memcpy(log_entry->path, kModulePath, log_entry->path_len + 1);

    tracker += entry_size;
  }
  return tracker - buffer;
}

bool RegisterLogNotification(HANDLE event_handle) {
  return true;
}
