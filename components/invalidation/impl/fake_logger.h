// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_LOGGER_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_LOGGER_H_

#include "google/cacheinvalidation/deps/logging.h"
#include "google/cacheinvalidation/include/system-resources.h"

namespace invalidation {

// A simple logger implementation for testing.
class FakeLogger : public Logger {
 public:
  ~FakeLogger() override;

  // Logger
  void Log(LogLevel level,
           const char* file,
           int line,
           const char* format,
           ...) override;

  void SetSystemResources(SystemResources* resources) override;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_LOGGER_H_
