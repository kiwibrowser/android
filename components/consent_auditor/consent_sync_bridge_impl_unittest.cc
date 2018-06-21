// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_sync_bridge_impl.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using sync_pb::UserConsentSpecifics;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::IsNull;
using testing::NotNull;
using testing::Pair;
using testing::Pointee;
using testing::Property;
using testing::Return;
using testing::SaveArg;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::WithArg;
using WriteBatch = ModelTypeStore::WriteBatch;

MATCHER_P(MatchesUserConsent, expected, "") {
  if (!arg.has_user_consent()) {
    *result_listener << "which is not a user consent";
    return false;
  }
  const UserConsentSpecifics& actual = arg.user_consent();
  if (actual.client_consent_time_usec() !=
      expected.client_consent_time_usec()) {
    return false;
  }
  return true;
}

UserConsentSpecifics CreateSpecifics(int64_t client_consent_time_usec) {
  UserConsentSpecifics specifics;
  specifics.set_client_consent_time_usec(client_consent_time_usec);
  specifics.set_account_id("account_id");
  return specifics;
}

std::unique_ptr<UserConsentSpecifics> SpecificsUniquePtr(
    int64_t client_consent_time_usec) {
  return std::make_unique<UserConsentSpecifics>(
      CreateSpecifics(client_consent_time_usec));
}

class ConsentSyncBridgeImplTest : public testing::Test {
 protected:
  ConsentSyncBridgeImplTest() {
    bridge_ = std::make_unique<ConsentSyncBridgeImpl>(
        ModelTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        mock_processor_.CreateForwardingProcessor(),
        GetAuthenticatedAccountIdCallback());
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  }

  std::string GetStorageKey(const UserConsentSpecifics& specifics) {
    EntityData entity_data;
    *entity_data.specifics.mutable_user_consent() = specifics;
    return bridge()->GetStorageKey(entity_data);
  }

  void SetAuthenticatedAccountId(const std::string& new_id) {
    authenticated_account_id_ = new_id;
  }

  std::string GetAuthenticatedAccountId() const {
    return authenticated_account_id_;
  }

  base::RepeatingCallback<std::string()> GetAuthenticatedAccountIdCallback()
      const {
    return base::BindRepeating(
        &ConsentSyncBridgeImplTest::GetAuthenticatedAccountId,
        base::Unretained(this));
  }

  ConsentSyncBridgeImpl* bridge() { return bridge_.get(); }
  MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  std::map<std::string, sync_pb::EntitySpecifics> GetAllData() {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetAllDataForDebugging(base::BindOnce(
        [](base::RunLoop* loop, std::unique_ptr<DataBatch>* out_batch,
           std::unique_ptr<DataBatch> batch) {
          *out_batch = std::move(batch);
          loop->Quit();
        },
        &loop, &batch));
    loop.Run();
    EXPECT_NE(nullptr, batch);

    std::map<std::string, sync_pb::EntitySpecifics> storage_key_to_specifics;
    if (batch != nullptr) {
      while (batch->HasNext()) {
        const syncer::KeyAndData& pair = batch->Next();
        storage_key_to_specifics[pair.first] = pair.second->specifics;
      }
    }
    return storage_key_to_specifics;
  }

  std::unique_ptr<sync_pb::EntitySpecifics> GetData(
      const std::string& storage_key) {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetData(
        {storage_key},
        base::BindOnce(
            [](base::RunLoop* loop, std::unique_ptr<DataBatch>* out_batch,
               std::unique_ptr<DataBatch> batch) {
              *out_batch = std::move(batch);
              loop->Quit();
            },
            &loop, &batch));
    loop.Run();
    EXPECT_NE(nullptr, batch);

    std::unique_ptr<sync_pb::EntitySpecifics> specifics;
    if (batch != nullptr && batch->HasNext()) {
      const syncer::KeyAndData& pair = batch->Next();
      specifics =
          std::make_unique<sync_pb::EntitySpecifics>(pair.second->specifics);
      EXPECT_FALSE(batch->HasNext());
    }
    return specifics;
  }

 private:
  std::unique_ptr<ConsentSyncBridgeImpl> bridge_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  base::MessageLoop message_loop_;

  std::string authenticated_account_id_;
};

TEST_F(ConsentSyncBridgeImplTest, ShouldCallModelReadyToSyncOnStartup) {
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ConsentSyncBridgeImplTest, ShouldRecordSingleConsent) {
  const UserConsentSpecifics specifics(
      CreateSpecifics(/*client_consent_time_usec=*/1u));
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(specifics));

  EXPECT_THAT(GetData(storage_key), Pointee(MatchesUserConsent(specifics)));
  EXPECT_THAT(GetData("bogus"), IsNull());
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(storage_key, MatchesUserConsent(specifics))));
}

TEST_F(ConsentSyncBridgeImplTest, ShouldNotDeleteConsentsWhenSyncIsDisabled) {
  UserConsentSpecifics user_consent_specifics(
      CreateSpecifics(/*client_consent_time_usec=*/2u));
  bridge()->RecordConsent(
      std::make_unique<UserConsentSpecifics>(user_consent_specifics));
  ASSERT_THAT(GetAllData(), SizeIs(1));

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  // User consent specific must be persisted when sync is disabled.
  EXPECT_THAT(GetAllData(),
              ElementsAre(Pair(_, MatchesUserConsent(user_consent_specifics))));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldRecordMultipleConsentsAndDeduplicateByTime) {
  std::set<std::string> unique_storage_keys;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .Times(4)
      .WillRepeatedly(
          [&unique_storage_keys](const std::string& storage_key,
                                 std::unique_ptr<EntityData> entity_data,
                                 MetadataChangeList* metadata_change_list) {
            unique_storage_keys.insert(storage_key);
          });

  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/2u));

  EXPECT_EQ(2u, unique_storage_keys.size());
  EXPECT_THAT(GetAllData(), SizeIs(2));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldDeleteCommitedConsentsAfterApplySyncChanges) {
  std::string first_storage_key;
  std::string second_storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&first_storage_key)))
      .WillOnce(WithArg<0>(SaveArg<0>(&second_storage_key)));

  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/2u));
  ASSERT_THAT(GetAllData(), SizeIs(2));

  auto error_on_delete = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      {EntityChange::CreateDelete(first_storage_key)});
  EXPECT_FALSE(error_on_delete);
  EXPECT_THAT(GetAllData(), SizeIs(1));
  EXPECT_THAT(GetData(first_storage_key), IsNull());
  EXPECT_THAT(GetData(second_storage_key), NotNull());
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldRecordConsentsEvenBeforeProcessorInitialization) {
  // Processor initializations depends on the store initialization. The consent
  // may be recorded before the store is initialized.
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(false));
  // The consent must be recorded, but not propagated anywhere while the
  // initilization is in progress.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  bridge()->RecordConsent(SpecificsUniquePtr(/*client_consent_time_usec=*/1u));
  EXPECT_THAT(GetAllData(), IsEmpty());
}

// User consents should be buffered if the store and processor is not fully
// initialized.
TEST_F(ConsentSyncBridgeImplTest,
       ShouldSubmitBufferedConsentsWhenStoreIsInitialized) {
  // Wait until bridge() is ready to avoid interference with processor() mock.
  base::RunLoop().RunUntilIdle();

  UserConsentSpecifics first_consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  first_consent.set_account_id("account_id");
  UserConsentSpecifics second_consent =
      CreateSpecifics(/*client_consent_time_usec=*/2u);
  second_consent.set_account_id("account_id");

  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(false));
  ModelType store_init_type;
  ModelTypeStore::InitCallback store_init_callback;
  ConsentSyncBridgeImpl late_init_bridge(
      base::BindLambdaForTesting(
          [&](ModelType type, ModelTypeStore::InitCallback callback) {
            store_init_type = type;
            store_init_callback = std::move(callback);
          }),
      processor()->CreateForwardingProcessor(),
      GetAuthenticatedAccountIdCallback());

  // Record consent before the store is initialized.
  late_init_bridge.RecordConsent(
      std::make_unique<UserConsentSpecifics>(first_consent));

  // Initialize the store.
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  std::move(store_init_callback)
      .Run(/*error=*/base::nullopt,
           ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(store_init_type));
  base::RunLoop().RunUntilIdle();

  SetAuthenticatedAccountId("account_id");
  late_init_bridge.OnSyncStarting();

  // Record consent after initializaiton is done.
  late_init_bridge.RecordConsent(
      std::make_unique<UserConsentSpecifics>(second_consent));

  // Both the pre-initialization and post-initialization consents must be
  // handled after initialization as usual.
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(GetAllData(),
              UnorderedElementsAre(Pair(GetStorageKey(first_consent),
                                        MatchesUserConsent(first_consent)),
                                   Pair(GetStorageKey(second_consent),
                                        MatchesUserConsent(second_consent))));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldReportPreviouslyPersistedConsentsWhenSyncIsReenabled) {
  UserConsentSpecifics consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  consent.set_account_id("account_id");

  bridge()->RecordConsent(std::make_unique<UserConsentSpecifics>(consent));

  // User disables sync, hovewer, the consent hasn't been submitted yet. It is
  // preserved to be submitted when sync is re-enabled.
  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetAllData(), SizeIs(1));

  // Reenable sync.
  SetAuthenticatedAccountId("account_id");
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  bridge()->OnSyncStarting();

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();

  // The previously preserved consent should be resubmitted to the processor
  // when sync is re-enabled.
  EXPECT_THAT(storage_key, Eq(GetStorageKey(consent)));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldReportPersistedConsentsOnStartupEvenWithLateStoreInitialization) {
  // Wait until bridge() is ready to avoid interference with processor() mock.
  base::RunLoop().RunUntilIdle();

  UserConsentSpecifics consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  consent.set_account_id("account_id");

  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(false));
  ModelType store_init_type;
  ModelTypeStore::InitCallback store_init_callback;
  ConsentSyncBridgeImpl late_init_bridge(
      base::BindLambdaForTesting(
          [&](ModelType type, ModelTypeStore::InitCallback callback) {
            store_init_type = type;
            store_init_callback = std::move(callback);
          }),
      processor()->CreateForwardingProcessor(),
      GetAuthenticatedAccountIdCallback());

  // Sync is active, but the store is not ready yet.
  SetAuthenticatedAccountId("account_id");
  EXPECT_CALL(*processor(), ModelReadyToSync(_)).Times(0);
  late_init_bridge.OnSyncStarting();

  // Initialize the store.
  std::unique_ptr<ModelTypeStore> store =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(store_init_type);

  // TODO(vitaliii): Try to avoid putting the data directly into the store (e.g.
  // by using a forwarding store), because this is an implementation detail.
  // However, currently the bridge owns the store and there is no obvious way to
  // preserve it.

  // Put the consent manually to simulate a restart with disabled sync.
  auto batch = store->CreateWriteBatch();
  batch->WriteData(GetStorageKey(consent), consent.SerializeAsString());
  store->CommitWriteBatch(std::move(batch), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  std::move(store_init_callback)
      .Run(/*error=*/base::nullopt,
           ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(store_init_type));

  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();

  // The previously preserved consent should be resubmitted to the processor
  // when the store is initilized, because the sync has already been re-enabled.
  EXPECT_THAT(storage_key, Eq(GetStorageKey(consent)));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldReportPersistedConsentsOnStartupEvenWithLateSyncInitialization) {
  // Wait until bridge() is ready to avoid interference with processor() mock.
  base::RunLoop().RunUntilIdle();

  UserConsentSpecifics consent =
      CreateSpecifics(/*client_consent_time_usec=*/1u);
  consent.set_account_id("account_id");

  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(false));
  ModelType store_init_type;
  ModelTypeStore::InitCallback store_init_callback;
  ConsentSyncBridgeImpl late_init_bridge(
      base::BindLambdaForTesting(
          [&](ModelType type, ModelTypeStore::InitCallback callback) {
            store_init_type = type;
            store_init_callback = std::move(callback);
          }),
      processor()->CreateForwardingProcessor(),
      GetAuthenticatedAccountIdCallback());

  // Initialize the store.
  std::unique_ptr<ModelTypeStore> store =
      ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(store_init_type);

  // TODO(vitaliii): Try to avoid putting the data directly into the store (e.g.
  // by using a forwarding store), because this is an implementation detail.
  // However, currently the bridge owns the store and there is no obvious way to
  // preserve it.

  // Put the consent manually to simulate a restart with disabled sync.
  auto batch = store->CreateWriteBatch();
  batch->WriteData(GetStorageKey(consent), consent.SerializeAsString());
  store->CommitWriteBatch(std::move(batch), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // The store has been initialized, but the sync is not active yet.
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  EXPECT_CALL(*processor(), ModelReadyToSync(NotNull()));
  std::move(store_init_callback)
      .Run(/*error=*/base::nullopt,
           ModelTypeStoreTestUtil::CreateInMemoryStoreForTest(store_init_type));
  base::RunLoop().RunUntilIdle();

  SetAuthenticatedAccountId("account_id");
  late_init_bridge.OnSyncStarting();

  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();

  // The previously preserved consent should be resubmitted to the processor
  // when sync is re-enabled, because the store has already been initialized.
  EXPECT_THAT(storage_key, Eq(GetStorageKey(consent)));
}

TEST_F(ConsentSyncBridgeImplTest,
       ShouldResubmitPersistedConsentOnlyIfSameAccount) {
  SetAuthenticatedAccountId("first_account");
  UserConsentSpecifics user_consent_specifics(
      CreateSpecifics(/*client_consent_time_usec=*/2u));
  user_consent_specifics.set_account_id("first_account");
  bridge()->RecordConsent(
      std::make_unique<UserConsentSpecifics>(user_consent_specifics));
  ASSERT_THAT(GetAllData(), SizeIs(1));

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  // The bridge may asynchronously query the store to choose what to delete.
  base::RunLoop().RunUntilIdle();

  ASSERT_THAT(GetAllData(),
              ElementsAre(Pair(_, MatchesUserConsent(user_consent_specifics))));

  // A new user signs in and enables sync.
  SetAuthenticatedAccountId("second_account");

  // The previous account consent should not be resubmited, because the new sync
  // account is different.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);
  ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  bridge()->OnSyncStarting();
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      bridge()->ApplyStopSyncChanges(WriteBatch::CreateMetadataChangeList()),
      Eq(ModelTypeSyncBridge::StopSyncResponse::kModelStillReadyToSync));
  base::RunLoop().RunUntilIdle();

  // The previous user signs in again and enables sync.
  SetAuthenticatedAccountId("first_account");

  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(WithArg<0>(SaveArg<0>(&storage_key)));
  bridge()->OnSyncStarting();
  // The bridge may asynchronously query the store to choose what to resubmit.
  base::RunLoop().RunUntilIdle();

  // This time their consent should be resubmitted, because it is for the same
  // account.
  EXPECT_THAT(storage_key, Eq(GetStorageKey(user_consent_specifics)));
}

}  // namespace

}  // namespace syncer
