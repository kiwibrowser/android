// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/fuzzed_data_provider.h"
#include "chrome/browser/chromeos/smb_client/smb_url.h"

// This is a workaround for https://crbug.com/778929.
struct IcuEnvironment {
  IcuEnvironment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    CHECK(base::i18n::InitializeICU());
  }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider fuzzed_data(data, size);

  chromeos::smb_client::SmbUrl url(fuzzed_data.ConsumeRemainingBytes());
  return 0;
}
