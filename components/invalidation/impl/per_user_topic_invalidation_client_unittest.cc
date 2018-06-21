// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_invalidation_client.h"

#include "components/invalidation/impl/fake_logger.h"
#include "components/invalidation/impl/fake_system_resources.h"
#include "google/cacheinvalidation/deps/logging.h"
#include "google/cacheinvalidation/include/invalidation-listener.h"
#include "google/cacheinvalidation/include/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class MockInvalidationListener : public InvalidationListener {
 public:
  MOCK_METHOD1(Ready, void(InvalidationClient*));
  MOCK_METHOD3(Invalidate,
               void(InvalidationClient*,
                    const Invalidation&,
                    const AckHandle&));
  MOCK_METHOD3(InvalidateUnknownVersion,
               void(InvalidationClient*, const ObjectId&, const AckHandle&));
  MOCK_METHOD2(InvalidateAll, void(InvalidationClient*, const AckHandle&));
  MOCK_METHOD3(InformRegistrationStatus,
               void(InvalidationClient*, const ObjectId&, RegistrationState));
  MOCK_METHOD4(InformRegistrationFailure,
               void(InvalidationClient*, const ObjectId&, bool, const string&));
  MOCK_METHOD3(ReissueRegistrations,
               void(InvalidationClient*, const string&, int));
  MOCK_METHOD2(InformError, void(InvalidationClient*, const ErrorInfo&));
};

// A mock of the Network interface.
class MockNetwork : public NetworkChannel {
 public:
  MOCK_METHOD1(SendMessage, void(const string&));
  MOCK_METHOD1(SetMessageReceiver, void(MessageCallback*));
  MOCK_METHOD1(AddNetworkStatusReceiver, void(NetworkStatusCallback*));
  MOCK_METHOD1(SetSystemResources, void(SystemResources*));
};

// A mock of the Storage interface.
class MockStorage : public Storage {
 public:
  MOCK_METHOD3(WriteKey, void(const string&, const string&, WriteKeyCallback*));
  MOCK_METHOD2(ReadKey, void(const string&, ReadKeyCallback*));
  MOCK_METHOD2(DeleteKey, void(const string&, DeleteKeyCallback*));
  MOCK_METHOD1(ReadAllKeys, void(ReadAllKeysCallback*));
  MOCK_METHOD1(SetSystemResources, void(SystemResources*));
};

// Tests the basic functionality of the invalidation client.
class InvalidationClientImplTest : public testing::Test {
 public:
  ~InvalidationClientImplTest() override {}

  // Performs setup for protocol handler unit tests, e.g. creating resource
  // components and setting up common expectations for certain mock objects.
  void SetUp() override {
    InitSystemResources();
    client_ = std::make_unique<PerUserTopicInvalidationClient>(resources_.get(),
                                                               &listener_);
  }

  void InitSystemResources() {
    // Create the actual resources.
    resources_ = std::make_unique<FakeSystemResources>(
        std::make_unique<FakeLogger>(), std::make_unique<MockNetwork>(),
        std::make_unique<MockStorage>(), "unit-test");
  }

  // The client being tested. Created fresh for each test function.
  std::unique_ptr<InvalidationClient> client_;

  // A mock invalidation listener.
  MockInvalidationListener listener_;
  std::unique_ptr<FakeSystemResources> resources_;
};

// Starts the ticl and checks that appropriate calls are made on the listener
// and that a proper message is sent on the network. Test is disabled on ASAN
// because using callback mechanism from cacheinvalidations library introduces a
// leak.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_Start DISABLED_Start
#else
#define MAYBE_Start Start
#endif
TEST_F(InvalidationClientImplTest, MAYBE_Start) {
  // Expect the listener to indicate that it is ready.
  EXPECT_CALL(listener_, Ready(testing::Eq(client_.get())));
  client_->Start();
}

}  // namespace invalidation
