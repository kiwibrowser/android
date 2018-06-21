// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_MOCK_H__
#define COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_MOCK_H__

#include <memory>
#include <string>

#include "components/sync/base/model_type.h"
#include "components/sync/device_info/local_device_info_provider.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/data_type_manager.h"
#include "components/sync/driver/model_associator.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/model/change_processor.h"
#include "components/sync/model/data_type_error_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncer {

class DataTypeEncryptionHandler;

class SyncApiComponentFactoryMock : public SyncApiComponentFactory {
 public:
  SyncApiComponentFactoryMock();
  ~SyncApiComponentFactoryMock() override;

  MOCK_METHOD2(CreateCommonDataTypeControllers,
               DataTypeController::TypeVector(
                   ModelTypeSet disabled_types,
                   LocalDeviceInfoProvider* local_device_info_provider));
  MOCK_METHOD6(CreateDataTypeManager,
               std::unique_ptr<DataTypeManager>(
                   ModelTypeSet,
                   const WeakHandle<DataTypeDebugInfoListener>&,
                   const DataTypeController::TypeMap*,
                   const DataTypeEncryptionHandler*,
                   ModelTypeConfigurer*,
                   DataTypeManagerObserver*));
  MOCK_METHOD4(CreateSyncEngine,
               std::unique_ptr<SyncEngine>(
                   const std::string& name,
                   invalidation::InvalidationService* invalidator,
                   const base::WeakPtr<SyncPrefs>& sync_prefs,
                   const base::FilePath& sync_folder));
  MOCK_METHOD0(CreateLocalDeviceInfoProvider,
               std::unique_ptr<LocalDeviceInfoProvider>());
  MOCK_METHOD1(
      CreateBookmarkSyncComponents,
      SyncComponents(std::unique_ptr<DataTypeErrorHandler> error_handler));
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_API_COMPONENT_FACTORY_MOCK_H__
