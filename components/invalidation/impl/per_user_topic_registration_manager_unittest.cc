// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_registration_manager.h"

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/invalidation/impl/json_unsafe_parser.h"
#include "components/invalidation/public/invalidation_util.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace syncer {

namespace {

size_t kInvalidationObjectIdsCount = 5;

const char kInvalidationRegistrationScope[] =
    "https://firebaseperusertopics-pa.googleapis.com";

const char kProjectId[] = "8181035976";

const char kTypeRegisteredForInvalidation[] =
    "invalidation.registered_for_invalidation";

invalidation::InvalidationObjectId GetIdForIndex(size_t index) {
  char name[2] = "a";
  name[0] += static_cast<char>(index);
  return invalidation::InvalidationObjectId(1 + index, name);
}

InvalidationObjectIdSet GetSequenceOfIdsStartingAt(size_t start, size_t count) {
  InvalidationObjectIdSet ids;
  for (size_t i = start; i < start + count; ++i)
    ids.insert(GetIdForIndex(i));
  return ids;
}

InvalidationObjectIdSet GetSequenceOfIds(size_t count) {
  return GetSequenceOfIdsStartingAt(0, count);
}

network::ResourceResponseHead CreateHeadersForTest(int responce_code) {
  network::ResourceResponseHead head;
  head.headers = new net::HttpResponseHeaders(base::StringPrintf(
      "HTTP/1.1 %d OK\nContent-type: text/html\n\n", responce_code));
  head.mime_type = "text/html";
  return head;
}

GURL FullUrl() {
  return GURL(base::StringPrintf(
      "%s/v1/perusertopics/%s/rel/topics/?subscriber_token=%s",
      kInvalidationRegistrationScope, kProjectId, "instance_id_token"));
}

network::URLLoaderCompletionStatus CreateStatusForTest(
    int status,
    const std::string& response_body) {
  network::URLLoaderCompletionStatus response_status(status);
  response_status.decoded_body_length = response_body.size();
  return response_status;
}

};  // namespace

class PerUserTopicRegistrationManagerTest : public testing::Test {
 protected:
  PerUserTopicRegistrationManagerTest() {}

  ~PerUserTopicRegistrationManagerTest() override {}

  void SetUp() override {
    PerUserTopicRegistrationManager::RegisterProfilePrefs(
        pref_service_.registry());
  }

  std::unique_ptr<PerUserTopicRegistrationManager> BuildRegistrationManager() {
    return std::make_unique<PerUserTopicRegistrationManager>(
        "instance_id_token", "access_token", &pref_service_,
        url_loader_factory(),
        base::BindRepeating(&syncer::JsonUnsafeParser::Parse));
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  base::MessageLoop message_loop_;
  network::TestURLLoaderFactory url_loader_factory_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicRegistrationManagerTest);
};

TEST_F(PerUserTopicRegistrationManagerTest,
       EmptyPrivateTopicShouldNotUpdateRegisteredIds) {
  InvalidationObjectIdSet ids = GetSequenceOfIds(kInvalidationObjectIdsCount);

  auto per_user_topic_registration_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());

  // Empty response body should result in no succesfull registrations.
  std::string response_body;

  url_loader_factory()->AddResponse(
      FullUrl(), CreateHeadersForTest(net::HTTP_OK), response_body,
      CreateStatusForTest(net::OK, response_body));

  per_user_topic_registration_manager->UpdateRegisteredIds(ids);
  base::RunLoop().RunUntilIdle();

  // The response didn't contain non-empty topic name. So nothing was
  // registered.
  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());
}

TEST_F(PerUserTopicRegistrationManagerTest, ShouldUpdateRegisteredIds) {
  InvalidationObjectIdSet ids = GetSequenceOfIds(kInvalidationObjectIdsCount);

  auto per_user_topic_registration_manager = BuildRegistrationManager();

  EXPECT_TRUE(per_user_topic_registration_manager->GetRegisteredIds().empty());

  std::string response_body = R"(
    {
      "private_topic_name": "test-pr"
    }
  )";

  url_loader_factory()->AddResponse(
      FullUrl(), CreateHeadersForTest(net::HTTP_OK), response_body,
      CreateStatusForTest(net::OK, response_body));

  per_user_topic_registration_manager->UpdateRegisteredIds(ids);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ids, per_user_topic_registration_manager->GetRegisteredIds());

  for (const auto& id : ids) {
    std::string topic_name = id.name();
    const base::DictionaryValue* topics =
        pref_service()->GetDictionary(kTypeRegisteredForInvalidation);
    const base::Value* private_topic_value =
        topics->FindKeyOfType(topic_name, base::Value::Type::STRING);
    ASSERT_NE(private_topic_value, nullptr);
  }
}

}  // namespace syncer
