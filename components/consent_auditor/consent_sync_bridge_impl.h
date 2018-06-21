// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/consent_auditor/consent_sync_bridge.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

class ConsentSyncBridgeImpl : public ConsentSyncBridge,
                              public ModelTypeSyncBridge {
 public:
  ConsentSyncBridgeImpl(
      OnceModelTypeStoreFactory store_factory,
      std::unique_ptr<ModelTypeChangeProcessor> change_processor,
      base::RepeatingCallback<std::string()> authenticated_account_id_callback);
  ~ConsentSyncBridgeImpl() override;

  // ModelTypeSyncBridge implementation.
  void OnSyncStarting() override;
  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  base::Optional<ModelError> MergeSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override;
  base::Optional<ModelError> ApplySyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
  StopSyncResponse ApplyStopSyncChanges(
      std::unique_ptr<MetadataChangeList> delete_metadata_change_list) override;

  // ConsentSyncBridge implementation.
  void RecordConsent(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics) override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetControllerDelegateOnUIThread() override;

 private:
  void RecordConsentImpl(
      std::unique_ptr<sync_pb::UserConsentSpecifics> specifics);
  // Record events in the deferred queue and clear the queue.
  void ProcessQueuedEvents();

  void OnStoreCreated(const base::Optional<ModelError>& error,
                      std::unique_ptr<ModelTypeStore> store);
  void OnReadAllMetadata(const base::Optional<ModelError>& error,
                         std::unique_ptr<MetadataBatch> metadata_batch);
  void OnCommit(const base::Optional<ModelError>& error);
  void OnReadData(DataCallback callback,
                  const base::Optional<ModelError>& error,
                  std::unique_ptr<ModelTypeStore::RecordList> data_records,
                  std::unique_ptr<ModelTypeStore::IdList> missing_id_list);
  void OnReadAllData(DataCallback callback,
                     const base::Optional<ModelError>& error,
                     std::unique_ptr<ModelTypeStore::RecordList> data_records);

  // Resubmit all the consents persisted in the store to sync consents, which
  // were preserved when sync was disabled. This may resubmit entities that the
  // processor already knows about (i.e. with metadata), but it is allowed.
  void ReadAllDataAndResubmit();
  void OnReadAllDataToResubmit(
      const base::Optional<ModelError>& error,
      std::unique_ptr<ModelTypeStore::RecordList> data_records);

  // Persistent storage for in flight consents. Should remain quite small, as we
  // delete upon commit confirmation. May contain consents without metadata
  // (e.g. persisted when sync was disabled).
  std::unique_ptr<ModelTypeStore> store_;

  // Used to store consents while the store or change processor are not
  // ready.
  std::vector<std::unique_ptr<sync_pb::UserConsentSpecifics>>
      deferred_consents_while_initializing_;

  base::RepeatingCallback<std::string()> authenticated_account_id_callback_;

  bool is_sync_starting_or_started_;

  DISALLOW_COPY_AND_ASSIGN(ConsentSyncBridgeImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_SYNC_BRIDGE_IMPL_H_
