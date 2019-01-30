// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_THIRD_PARTY_DLLS_HOOK_H_
#define CHROME_ELF_THIRD_PARTY_DLLS_HOOK_H_

#include <windows.h>

#include <string>

namespace third_party_dlls {

// "static_cast<int>(HookStatus::value)" to access underlying value.
enum class HookStatus {
  kSuccess = 0,
  kInitImportsFailure = 1,
  kUnsupportedOs = 2,
  kVirtualProtectFail = 3,
  kApplyHookFail = 4,
  COUNT
};

// Apply hook.
// - Ensure the rest of third_party_dlls is initialized before hooking.
HookStatus ApplyHook();

// Testing-only access to private GetDataFromImage() function.
bool GetDataFromImageForTesting(PVOID mapped_image,
                                DWORD* time_date_stamp,
                                DWORD* image_size,
                                std::string* image_name,
                                std::string* section_path,
                                std::string* section_basename);

}  // namespace third_party_dlls

#endif  // CHROME_ELF_THIRD_PARTY_DLLS_HOOK_H_
