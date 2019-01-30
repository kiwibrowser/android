// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_THIRD_PARTY_DLLS_MAIN_UNITTEST_EXE_H_
#define CHROME_ELF_THIRD_PARTY_DLLS_MAIN_UNITTEST_EXE_H_

namespace third_party_dlls {
namespace main_unittest_exe {

enum ExitCode {
  kDllLoadSuccess = 0,
  kDllLoadFailed = 1,
  // Unexpected failures are negative ints:
  kBadCommandLine = -1,
  kThirdPartyAlreadyInitialized = -2,
  kThirdPartyInitFailure = -3,
  kMissingArgument = -4,
  kBadBlacklistPath = -5,
  kBadArgument = -6,
  kUnsupportedTestId = -7,
  kEmptyLog = -8,
  kUnexpectedLog = -9,
};

enum TestId {
  kTestOnlyInitialization = 1,
  kTestSingleDllLoad = 2,
};

}  // namespace main_unittest_exe
}  // namespace third_party_dlls

#endif  // CHROME_ELF_THIRD_PARTY_DLLS_MAIN_UNITTEST_EXE_H_
