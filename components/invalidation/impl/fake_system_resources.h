// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_FAKE_SYSTEM_RESOURCES_H_
#define COMPONENTS_INVALIDATION_IMPL_FAKE_SYSTEM_RESOURCES_H_

#include "google/cacheinvalidation/include/system-resources.h"
#include "google/cacheinvalidation/include/types.h"

namespace invalidation {

class Logger;
class NetworkChannel;
class Scheduler;
class Storage;
class SystemResources;

class FakeSystemResources : public SystemResources {
 public:
  FakeSystemResources(std::unique_ptr<Logger> logger,
                      std::unique_ptr<NetworkChannel> network,
                      std::unique_ptr<Storage> storage,
                      const string& platform);

  ~FakeSystemResources() override;

  // Overrides from SystemResources.
  void Start() override;
  void Stop() override;
  bool IsStarted() const override;

  Logger* logger() override;
  Scheduler* internal_scheduler() override;
  Scheduler* listener_scheduler() override;
  NetworkChannel* network() override;
  Storage* storage() override;
  string platform() const override;

 private:
  // Components comprising the system resources. We delegate calls to these as
  // appropriate.
  std::unique_ptr<Logger> logger_;
  std::unique_ptr<NetworkChannel> network_;
  std::unique_ptr<Storage> storage_;

  // Information about the client operating system/platform.
  string platform_;
  bool is_started_ = false;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_FAKE_SYSTEM_RESOURCES_H_
