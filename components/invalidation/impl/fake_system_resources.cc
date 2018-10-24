// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fake_system_resources.h"

#include "google/cacheinvalidation/deps/string_util.h"
#include "google/cacheinvalidation/include/system-resources.h"

namespace invalidation {

FakeSystemResources::FakeSystemResources(
    std::unique_ptr<Logger> logger,
    std::unique_ptr<NetworkChannel> network,
    std::unique_ptr<Storage> storage,
    const string& platform)
    : logger_(std::move(logger)),
      network_(std::move(network)),
      storage_(std::move(storage)),
      platform_(platform) {
  logger_->SetSystemResources(this);
  network_->SetSystemResources(this);
  storage_->SetSystemResources(this);
}

FakeSystemResources::~FakeSystemResources() {}

void FakeSystemResources::Start() {
  is_started_ = true;
}

void FakeSystemResources::Stop() {
  CHECK(is_started_) << "cannot stop resources that aren't started";
  is_started_ = false;
}

bool FakeSystemResources::IsStarted() const {
  return is_started_;
}

Logger* FakeSystemResources::logger() {
  return logger_.get();
}

Scheduler* FakeSystemResources::internal_scheduler() {
  return nullptr;
}

Scheduler* FakeSystemResources::listener_scheduler() {
  return nullptr;
}

NetworkChannel* FakeSystemResources::network() {
  return network_.get();
}

Storage* FakeSystemResources::storage() {
  return storage_.get();
}

string FakeSystemResources::platform() const {
  return platform_;
}

}  // namespace invalidation
