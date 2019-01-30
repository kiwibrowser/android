// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/consent_auditor/consent_sync_bridge_impl.h"

#include <set>
#include <utility>
#include <vector>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using sync_pb::UserConsentSpecifics;
using IdList = ModelTypeStore::IdList;
using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

std::string GetStorageKeyFromSpecifics(const UserConsentSpecifics& specifics) {
  // Force Big Endian, this means newly created keys are last in sort order,
  // which allows leveldb to append new writes, which it is best at.
  // TODO(skym): Until we force |event_time_usec| to never conflict, this has
  // the potential for errors.
  std::string key(8, 0);
  base::WriteBigEndian(&key[0], specifics.client_consent_time_usec());
  return key;
}

std::unique_ptr<EntityData> MoveToEntityData(
    std::unique_ptr<UserConsentSpecifics> specifics) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->non_unique_name =
      base::Int64ToString(specifics->client_consent_time_usec());
  entity_data->specifics.set_allocated_user_consent(specifics.release());
  return entity_data;
}

// TODO(vitaliii): Delete this function both here and in UserEventSyncBridge.
std::unique_ptr<EntityData> CopyToEntityData(
    const UserConsentSpecifics specifics) {
  auto entity_data = std::make_unique<EntityData>();
  entity_data->non_unique_name =
      base::Int64ToString(specifics.client_consent_time_usec());
  *entity_data->specifics.mutable_user_consent() = specifics;
  return entity_data;
}

}  // namespace

ConsentSyncBridgeImpl::ConsentSyncBridgeImpl(
    OnceModelTypeStoreFactory store_factory,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor,
    base::RepeatingCallback<std::string()> authenticated_account_id_callback)
    : ModelTypeSyncBridge(std::move(change_processor)),
      authenticated_account_id_callback_(authenticated_account_id_callback),
      is_sync_starting_or_started_(false) {
  DCHECK(authenticated_account_id_callback_);
  // TODO(vitaliii): Use USER_CONSENTS once the new type is added.
  std::move(store_factory)
      .Run(USER_EVENTS, base::BindOnce(&ConsentSyncBridgeImpl::OnStoreCreated,
                                       base::AsWeakPtr(this)));
}

ConsentSyncBridgeImpl::~ConsentSyncBridgeImpl() {
  if (!deferred_consents_while_initializing_.empty())
    LOG(ERROR) << "Non-empty event queue at shutdown!";
}

std::unique_ptr<MetadataChangeList>
ConsentSyncBridgeImpl::CreateMetadataChangeList() {
  return WriteBatch::CreateMetadataChangeList();
}

base::Optional<ModelError> ConsentSyncBridgeImpl::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  NOTREACHED();
  return {};
}

base::Optional<ModelError> ConsentSyncBridgeImpl::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (EntityChange& change : entity_changes) {
    DCHECK_EQ(EntityChange::ACTION_DELETE, change.type());
    batch->DeleteData(change.storage_key());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ConsentSyncBridgeImpl::OnCommit, base::AsWeakPtr(this)));
  return {};
}

void ConsentSyncBridgeImpl::GetData(StorageKeyList storage_keys,
                                    DataCallback callback) {
  store_->ReadData(storage_keys,
                   base::BindOnce(&ConsentSyncBridgeImpl::OnReadData,
                                  base::AsWeakPtr(this), std::move(callback)));
}

void ConsentSyncBridgeImpl::GetAllDataForDebugging(DataCallback callback) {
  store_->ReadAllData(base::BindOnce(&ConsentSyncBridgeImpl::OnReadAllData,
                                     base::AsWeakPtr(this),
                                     std::move(callback)));
}

std::string ConsentSyncBridgeImpl::GetClientTag(const EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string ConsentSyncBridgeImpl::GetStorageKey(
    const EntityData& entity_data) {
  return GetStorageKeyFromSpecifics(entity_data.specifics.user_consent());
}

void ConsentSyncBridgeImpl::OnSyncStarting() {
#if !defined(OS_IOS)  // https://crbug.com/834042
  DCHECK(!authenticated_account_id_callback_.Run().empty());
#endif  // !defined(OS_IOS)
  DCHECK(!is_sync_starting_or_started_);

  is_sync_starting_or_started_ = true;
  if (store_ && change_processor()->IsTrackingMetadata()) {
    ReadAllDataAndResubmit();
  }
}

ModelTypeSyncBridge::StopSyncResponse
ConsentSyncBridgeImpl::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // Sync can only be stopped after initialization.
  DCHECK(deferred_consents_while_initializing_.empty());

  is_sync_starting_or_started_ = false;

  if (delete_metadata_change_list) {
    // Preserve all consents in the store, but delete their metadata, because it
    // may become invalid when the sync is reenabled. It is important to report
    // all user consents, thus, they are persisted for some time even after
    // signout. We will try to resubmit these consents once the sync is enabled
    // again. This may lead to same consent being submitted multiple times, but
    // this is allowed.
    std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
    batch->TakeMetadataChangesFrom(std::move(delete_metadata_change_list));

    store_->CommitWriteBatch(std::move(batch),
                             base::BindOnce(&ConsentSyncBridgeImpl::OnCommit,
                                            base::AsWeakPtr(this)));
  }

  return StopSyncResponse::kModelStillReadyToSync;
}

void ConsentSyncBridgeImpl::ReadAllDataAndResubmit() {
  DCHECK(is_sync_starting_or_started_);
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(store_);
  store_->ReadAllData(base::BindOnce(
      &ConsentSyncBridgeImpl::OnReadAllDataToResubmit, base::AsWeakPtr(this)));
}

void ConsentSyncBridgeImpl::OnReadAllDataToResubmit(
    const base::Optional<ModelError>& error,
    std::unique_ptr<RecordList> data_records) {
  if (!is_sync_starting_or_started_) {
    // Meanwhile the sync has been disabled. We will try next time.
    return;
  }
  DCHECK(change_processor()->IsTrackingMetadata());

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const Record& r : *data_records) {
    auto specifics = std::make_unique<UserConsentSpecifics>();
    if (specifics->ParseFromString(r.value) &&
        specifics->account_id() == authenticated_account_id_callback_.Run()) {
      RecordConsentImpl(std::move(specifics));
    }
  }
}

void ConsentSyncBridgeImpl::RecordConsent(
    std::unique_ptr<UserConsentSpecifics> specifics) {
  DCHECK(!specifics->account_id().empty());
  if (change_processor()->IsTrackingMetadata()) {
    RecordConsentImpl(std::move(specifics));
    return;
  }
  deferred_consents_while_initializing_.push_back(std::move(specifics));
}

void ConsentSyncBridgeImpl::RecordConsentImpl(
    std::unique_ptr<UserConsentSpecifics> specifics) {
  DCHECK(store_);
  DCHECK(change_processor()->IsTrackingMetadata());

  std::string storage_key = GetStorageKeyFromSpecifics(*specifics);
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  batch->WriteData(storage_key, specifics->SerializeAsString());

  change_processor()->Put(storage_key, MoveToEntityData(std::move(specifics)),
                          batch->GetMetadataChangeList());
  store_->CommitWriteBatch(
      std::move(batch),
      base::BindOnce(&ConsentSyncBridgeImpl::OnCommit, base::AsWeakPtr(this)));
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ConsentSyncBridgeImpl::GetControllerDelegateOnUIThread() {
  return change_processor()->GetControllerDelegateOnUIThread();
}

void ConsentSyncBridgeImpl::ProcessQueuedEvents() {
  DCHECK(change_processor()->IsTrackingMetadata());
  for (std::unique_ptr<sync_pb::UserConsentSpecifics>& event :
       deferred_consents_while_initializing_) {
    RecordConsentImpl(std::move(event));
  }
  deferred_consents_while_initializing_.clear();
}

void ConsentSyncBridgeImpl::OnStoreCreated(
    const base::Optional<ModelError>& error,
    std::unique_ptr<ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  // TODO(vitaliii): Garbage collect old consents if sync is disabled.

  store_ = std::move(store);
  store_->ReadAllMetadata(base::BindOnce(
      &ConsentSyncBridgeImpl::OnReadAllMetadata, base::AsWeakPtr(this)));
}

void ConsentSyncBridgeImpl::OnReadAllMetadata(
    const base::Optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
  } else {
    change_processor()->ModelReadyToSync(std::move(metadata_batch));
    DCHECK(change_processor()->IsTrackingMetadata());
    if (is_sync_starting_or_started_) {
      ReadAllDataAndResubmit();
    }
    ProcessQueuedEvents();
  }
}

void ConsentSyncBridgeImpl::OnCommit(const base::Optional<ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void ConsentSyncBridgeImpl::OnReadData(
    DataCallback callback,
    const base::Optional<ModelError>& error,
    std::unique_ptr<RecordList> data_records,
    std::unique_ptr<IdList> missing_id_list) {
  OnReadAllData(std::move(callback), error, std::move(data_records));
}

void ConsentSyncBridgeImpl::OnReadAllData(
    DataCallback callback,
    const base::Optional<ModelError>& error,
    std::unique_ptr<RecordList> data_records) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  UserConsentSpecifics specifics;
  for (const Record& r : *data_records) {
    if (specifics.ParseFromString(r.value)) {
      DCHECK_EQ(r.id, GetStorageKeyFromSpecifics(specifics));
      batch->Put(r.id, CopyToEntityData(specifics));
    } else {
      change_processor()->ReportError(
          {FROM_HERE, "Failed deserializing user events."});
      return;
    }
  }
  std::move(callback).Run(std::move(batch));
}

}  // namespace syncer
