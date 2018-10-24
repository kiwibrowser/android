// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_invalidation_client.h"

#include "base/bind.h"
#include "google/cacheinvalidation/deps/callback.h"
#include "google/cacheinvalidation/impl/log-macro.h"
#include "google/cacheinvalidation/include/invalidation-listener.h"

namespace invalidation {

PerUserTopicInvalidationClient::PerUserTopicInvalidationClient(
    SystemResources* resources,
    InvalidationListener* listener)
    : resources_(resources), listener_(listener) {
  RegisterWithNetwork(resources_);
  TLOG(logger(), INFO, "Created client");
}

PerUserTopicInvalidationClient::~PerUserTopicInvalidationClient() {}

void PerUserTopicInvalidationClient::RegisterWithNetwork(
    SystemResources* resources) {
  // Install ourselves as a receiver for server messages.
  resources->network()->SetMessageReceiver(NewPermanentCallback(
      this, &PerUserTopicInvalidationClient::MessageReceiver));
}

void PerUserTopicInvalidationClient::Start() {
  if (ticl_protocol_started_) {
    TLOG(logger(), SEVERE, "Ignoring start call since already started");
    return;
  }

  FinishStartingTiclAndInformListener();
}

void PerUserTopicInvalidationClient::Stop() {
  TLOG(logger(), INFO, "Ticl being stopped");
  ticl_protocol_started_ = false;
}

void PerUserTopicInvalidationClient::FinishStartingTiclAndInformListener() {
  DCHECK(!ticl_protocol_started_);
  ticl_protocol_started_ = true;
  GetListener()->Ready(this);
  GetListener()->ReissueRegistrations(this, "", 0);
  TLOG(logger(), INFO, "Ticl started");
}

void PerUserTopicInvalidationClient::MessageReceiver(string message) {
  // TODO(melandory): Here message should be passed to the protocol handler.
}

}  // namespace invalidation
