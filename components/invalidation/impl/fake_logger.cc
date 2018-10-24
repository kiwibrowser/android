// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_logger.h"

#include "google/cacheinvalidation/deps/string_util.h"

namespace invalidation {

FakeLogger::~FakeLogger() {}

void FakeLogger::Log(LogLevel level,
                     const char* file,
                     int line,
                     const char* format,
                     ...) {}

void FakeLogger::SetSystemResources(SystemResources* resources) {
  // Nothing to do (logger uses no other resources).
}

}  // namespace invalidation
