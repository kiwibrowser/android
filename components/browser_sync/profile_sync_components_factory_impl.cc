// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_components_factory_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_wallet_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/web_data_model_type_controller.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/history/core/browser/history_delete_directives_data_type_controller.h"
#include "components/history/core/browser/typed_url_model_type_controller.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/sync/browser/password_data_type_controller.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/local_device_info_provider_impl.h"
#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/driver/glue/sync_backend_host_impl.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/proxy_data_type_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync_bookmarks/bookmark_change_processor.h"
#include "components/sync_bookmarks/bookmark_data_type_controller.h"
#include "components/sync_bookmarks/bookmark_model_associator.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "components/sync_sessions/session_model_type_controller.h"

using base::FeatureList;
using bookmarks::BookmarkModel;
using sync_bookmarks::BookmarkChangeProcessor;
using sync_bookmarks::BookmarkDataTypeController;
using sync_bookmarks::BookmarkModelAssociator;
using sync_sessions::SessionDataTypeController;
using syncer::AsyncDirectoryTypeController;
using syncer::DataTypeController;
using syncer::DataTypeManager;
using syncer::DataTypeManagerImpl;
using syncer::DataTypeManagerObserver;
using syncer::ModelTypeController;
using syncer::ProxyDataTypeController;

namespace browser_sync {

namespace {

// This helper function only wraps
// autofill::AutocompleteSyncBridge::FromWebDataService(). This way, it
// simplifies life for the compiler which cannot directly cast
// "WeakPtr<ModelTypeSyncBridge> (AutofillWebDataService*)" to
// "WeakPtr<ModelTypeControllerDelegate> (AutofillWebDataService*)".
base::WeakPtr<syncer::ModelTypeControllerDelegate> DelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutocompleteSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegateOnUIThread();
}

}  // namespace

ProfileSyncComponentsFactoryImpl::ProfileSyncComponentsFactoryImpl(
    syncer::SyncClient* sync_client,
    version_info::Channel channel,
    const std::string& version,
    bool is_tablet,
    const char* history_disabled_pref,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service,
    const scoped_refptr<password_manager::PasswordStore>& password_store)
    : sync_client_(sync_client),
      channel_(channel),
      version_(version),
      is_tablet_(is_tablet),
      history_disabled_pref_(history_disabled_pref),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      web_data_service_(web_data_service),
      password_store_(password_store) {}

ProfileSyncComponentsFactoryImpl::~ProfileSyncComponentsFactoryImpl() {}

syncer::DataTypeController::TypeVector
ProfileSyncComponentsFactoryImpl::CreateCommonDataTypeControllers(
    syncer::ModelTypeSet disabled_types,
    syncer::LocalDeviceInfoProvider* local_device_info_provider) {
  syncer::DataTypeController::TypeVector controllers;
  base::Closure error_callback =
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel_);

  // TODO(stanisc): can DEVICE_INFO be one of disabled datatypes?
  // Use an error callback that always uploads a stacktrace if it can to help
  // get USS as stable as possible.
  controllers.push_back(std::make_unique<ModelTypeController>(
      syncer::DEVICE_INFO, sync_client_, ui_thread_));
  // These features are enabled only if there's a DB thread to post tasks to.
  if (db_thread_) {
    // Autocomplete sync is enabled by default.  Register unless explicitly
    // disabled.
    if (!disabled_types.Has(syncer::AUTOFILL)) {
      controllers.push_back(
          std::make_unique<autofill::WebDataModelTypeController>(
              syncer::AUTOFILL, sync_client_, db_thread_, web_data_service_,
              base::BindRepeating(&DelegateFromDataService)));
    }

    // Autofill sync is enabled by default.  Register unless explicitly
    // disabled.
    if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
      controllers.push_back(std::make_unique<AutofillProfileDataTypeController>(
          db_thread_, error_callback, sync_client_, web_data_service_));
    }

    // Wallet data sync is enabled by default, but behind a syncer experiment
    // enforced by the datatype controller. Register unless explicitly disabled.
    bool wallet_disabled = disabled_types.Has(syncer::AUTOFILL_WALLET_DATA);
    if (!wallet_disabled) {
      controllers.push_back(std::make_unique<AutofillWalletDataTypeController>(
          syncer::AUTOFILL_WALLET_DATA, db_thread_, error_callback,
          sync_client_, web_data_service_));
    }

    // Wallet metadata sync depends on Wallet data sync. Register if Wallet data
    // is syncing and metadata sync is not explicitly disabled.
    if (!wallet_disabled &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_METADATA)) {
      controllers.push_back(std::make_unique<AutofillWalletDataTypeController>(
          syncer::AUTOFILL_WALLET_METADATA, db_thread_, error_callback,
          sync_client_, web_data_service_));
    }
  }

  // Bookmark sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    if (FeatureList::IsEnabled(switches::kSyncUSSBookmarks)) {
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::BOOKMARKS, sync_client_, ui_thread_));
    } else {
      controllers.push_back(std::make_unique<BookmarkDataTypeController>(
          error_callback, sync_client_));
    }
  }

  // These features are enabled only if history is not disabled.
  if (!sync_client_->GetPrefService()->GetBoolean(history_disabled_pref_)) {
    // TypedUrl sync is enabled by default.  Register unless explicitly
    // disabled.
    if (!disabled_types.Has(syncer::TYPED_URLS)) {
      controllers.push_back(
          std::make_unique<history::TypedURLModelTypeController>(
              sync_client_, history_disabled_pref_));
    }

    // Delete directive sync is enabled by default.
    if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
      controllers.push_back(
          std::make_unique<HistoryDeleteDirectivesDataTypeController>(
              error_callback, sync_client_));
    }

    // Session sync is enabled by default.  This is disabled if history is
    // disabled because the tab sync data is added to the web history on the
    // server.
    if (!disabled_types.Has(syncer::PROXY_TABS)) {
      controllers.push_back(
          std::make_unique<ProxyDataTypeController>(syncer::PROXY_TABS));
      if (FeatureList::IsEnabled(switches::kSyncUSSSessions)) {
        controllers.push_back(
            std::make_unique<sync_sessions::SessionModelTypeController>(
                sync_client_, ui_thread_, history_disabled_pref_));
      } else {
        controllers.push_back(std::make_unique<SessionDataTypeController>(
            error_callback, sync_client_, local_device_info_provider,
            history_disabled_pref_));
      }
    }

    // Favicon sync is enabled by default. Register unless explicitly disabled.
    if (!disabled_types.Has(syncer::FAVICON_IMAGES) &&
        !disabled_types.Has(syncer::FAVICON_TRACKING)) {
      // crbug/384552. We disable error uploading for this data types for now.
      controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
          syncer::FAVICON_IMAGES, base::RepeatingClosure(), sync_client_,
          syncer::GROUP_UI, ui_thread_));
      controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
          syncer::FAVICON_TRACKING, base::RepeatingClosure(), sync_client_,
          syncer::GROUP_UI, ui_thread_));
    }
  }

  // Password sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PASSWORDS)) {
    controllers.push_back(std::make_unique<PasswordDataTypeController>(
        error_callback, sync_client_,
        sync_client_->GetPasswordStateChangedCallback(), password_store_));
  }

  if (!disabled_types.Has(syncer::PREFERENCES)) {
    if (!override_prefs_controller_to_uss_for_test_) {
      controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
          syncer::PREFERENCES, error_callback, sync_client_, syncer::GROUP_UI,
          ui_thread_));
    } else {
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::PREFERENCES, sync_client_, ui_thread_));
    }
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
        syncer::PRIORITY_PREFERENCES, error_callback, sync_client_,
        syncer::GROUP_UI, ui_thread_));
  }

  // Article sync is disabled by default.  Register only if explicitly enabled.
  if (dom_distiller::IsEnableSyncArticlesSet()) {
    controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
        syncer::ARTICLES, error_callback, sync_client_, syncer::GROUP_UI,
        ui_thread_));
  }

#if defined(OS_CHROMEOS)
  if (!disabled_types.Has(syncer::PRINTERS)) {
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::PRINTERS, sync_client_, ui_thread_));
  }
#endif

  // Reading list sync is enabled by default only on iOS. Register unless
  // Reading List or Reading List Sync is explicitly disabled.
  if (!disabled_types.Has(syncer::READING_LIST) &&
      reading_list::switches::IsReadingListEnabled()) {
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::READING_LIST, sync_client_, ui_thread_));
  }

  if (!disabled_types.Has(syncer::USER_EVENTS) &&
      FeatureList::IsEnabled(switches::kSyncUserEvents)) {
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::USER_EVENTS, sync_client_, ui_thread_));
  }

  if (base::FeatureList::IsEnabled(switches::kSyncUserConsentSeparateType)) {
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::USER_CONSENTS, sync_client_, ui_thread_));
  }

  return controllers;
}

std::unique_ptr<DataTypeManager>
ProfileSyncComponentsFactoryImpl::CreateDataTypeManager(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const syncer::DataTypeEncryptionHandler* encryption_handler,
    syncer::ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer) {
  return std::make_unique<DataTypeManagerImpl>(
      sync_client_, initial_types, debug_info_listener, controllers,
      encryption_handler, configurer, observer);
}

std::unique_ptr<syncer::SyncEngine>
ProfileSyncComponentsFactoryImpl::CreateSyncEngine(
    const std::string& name,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<syncer::SyncPrefs>& sync_prefs,
    const base::FilePath& sync_data_folder) {
  return std::make_unique<syncer::SyncBackendHostImpl>(
      name, sync_client_, invalidator, sync_prefs, sync_data_folder);
}

std::unique_ptr<syncer::LocalDeviceInfoProvider>
ProfileSyncComponentsFactoryImpl::CreateLocalDeviceInfoProvider() {
  return std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
      channel_, version_, is_tablet_);
}

syncer::SyncApiComponentFactory::SyncComponents
ProfileSyncComponentsFactoryImpl::CreateBookmarkSyncComponents(
    std::unique_ptr<syncer::DataTypeErrorHandler> error_handler) {
  BookmarkModel* bookmark_model = sync_client_->GetBookmarkModel();
  syncer::UserShare* user_share =
      sync_client_->GetSyncService()->GetUserShare();
// TODO(akalin): We may want to propagate this switch up eventually.
#if defined(OS_ANDROID) || defined(OS_IOS)
  const bool kExpectMobileBookmarksFolder = true;
#else
  const bool kExpectMobileBookmarksFolder = false;
#endif

  auto model_associator = std::make_unique<BookmarkModelAssociator>(
      bookmark_model, sync_client_, user_share, error_handler->Copy(),
      kExpectMobileBookmarksFolder);

  SyncComponents components;
  components.change_processor = std::make_unique<BookmarkChangeProcessor>(
      sync_client_, model_associator.get(), std::move(error_handler));
  components.model_associator = std::move(model_associator);
  return components;
}

// static
void ProfileSyncComponentsFactoryImpl::OverridePrefsForUssTest(bool use_uss) {
  override_prefs_controller_to_uss_for_test_ = use_uss;
}

bool ProfileSyncComponentsFactoryImpl::
    override_prefs_controller_to_uss_for_test_ = false;

}  // namespace browser_sync
