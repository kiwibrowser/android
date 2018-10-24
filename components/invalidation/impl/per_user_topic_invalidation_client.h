// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_INVALIDATION_CLIENT_H_
#define COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_INVALIDATION_CLIENT_H_

#include <string>
#include <utility>

#include "base/macros.h"
#include "google/cacheinvalidation/include/invalidation-client.h"
#include "google/cacheinvalidation/include/system-resources.h"
#include "google/cacheinvalidation/include/types.h"

namespace invalidation {

class InvalidationListener;
class Logger;

class PerUserTopicInvalidationClient : public InvalidationClient {
 public:
  PerUserTopicInvalidationClient(SystemResources* resources,
                                 InvalidationListener* listener);

  ~PerUserTopicInvalidationClient() override;

  /* Returns true iff the client is currently started. */
  bool IsStartedForTest() { return ticl_protocol_started_; }

  //  InvalidationClient implementation
  void Start() override;
  void Stop() override;
  void Register(const ObjectId& object_id) override {}
  void Unregister(const ObjectId& object_id) override {}
  void Register(const vector<ObjectId>& object_ids) override {}
  void Unregister(const vector<ObjectId>& object_ids) override {}
  void Acknowledge(const AckHandle& ack_handle) override {}

 private:
  InvalidationListener* GetListener() { return listener_; }
  Logger* logger() { return resources_->logger(); }

  /* Handles the result of a request to read from persistent storage. */
  void ReadCallback(std::pair<Status, std::string> read_result);

  /* Finish starting the ticl and inform the listener that it is ready. */
  void FinishStartingTiclAndInformListener();

  /* Registers a message receiver and status change listener on |resources|. */
  void RegisterWithNetwork(SystemResources* resources);

  /* Handles inbound messages from the network. */
  void MessageReceiver(std::string message);

  /* Resources for the Ticl. Owned by interface user */
  SystemResources* resources_;

  /* The state of the Ticl whether it has started or not. */
  bool ticl_protocol_started_ = false;

  InvalidationListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicInvalidationClient);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_IMPL_PER_USER_TOPIC_INVALIDATION_CLIENT_H_
