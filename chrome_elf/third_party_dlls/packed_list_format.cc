// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/third_party_dlls/packed_list_format.h"

#include <stddef.h>

namespace third_party_dlls {

// Subdir relative to install_static::GetUserDataDirectory().
const wchar_t kFileSubdir[] =
    L"\\ThirdPartyModuleList"
#if defined(_WIN64)
    L"64";
#else
    L"32";
#endif

// Packed module data cache file.
const wchar_t kBlFileName[] = L"\\bldata";

std::string GetFingerprintString(uint32_t image_size,
                                 uint32_t time_data_stamp) {
  // Max hex 32-bit value is 8 characters long.  2*8+1.
  char buffer[17] = {};
  ::snprintf(buffer, sizeof(buffer), "%08X%x", time_data_stamp, image_size);

  return std::string(buffer);
}

}  // namespace third_party_dlls
