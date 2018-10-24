// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string_piece_forward.h"
#include "chrome/browser/conflicts/module_blacklist_cache_updater_win.h"
#include "chrome/browser/conflicts/module_database_observer_win.h"
#include "components/component_updater/component_updater_service.h"

class IncompatibleApplicationsUpdater;
class InstalledApplications;
class ModuleListFilter;
class PrefRegistrySimple;
struct CertificateInfo;

namespace base {
class FilePath;
class SequencedTaskRunner;
class TaskRunner;
}

// This class owns all the third-party conflicts-related classes and is
// responsible for their initialization.
//
// The Module List component is received from the component update service,
// which invokes OnModuleListComponentRegister() and LoadModuleList() when
// appropriate.
class ThirdPartyConflictsManager
    : public ModuleDatabaseObserver,
      public component_updater::ComponentUpdateService::Observer {
 public:
  // |module_database_event_source| must outlive this.
  explicit ThirdPartyConflictsManager(
      ModuleDatabaseEventSource* module_database_event_source);
  ~ThirdPartyConflictsManager() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Explicitely disables the third-party module blocking feature. This is
  // needed because simply turning off the feature using either the Feature List
  // API or via group policy is not sufficient. Disabling the blocking requires
  // the deletion of the module blacklist cache. This task is executed on
  // |background_sequence|.
  static void DisableThirdPartyModuleBlocking(
      base::TaskRunner* background_sequence);

  // Explicitely disables the blocking of third-party modules for the next
  // browser launch and prevent |instance| from reenabling it. Basically calls
  // the above function in the background sequence of |instance| and then
  // deletes that instance.
  static void ShutdownAndDestroy(
      std::unique_ptr<ThirdPartyConflictsManager> instance);

  // ModuleDatabaseObserver:
  void OnModuleDatabaseIdle() override;

  // Invoked when the Third Party Module List component is registered with the
  // component update service. Checks if the component is currently installed or
  // if an update is required.
  void OnModuleListComponentRegistered(base::StringPiece component_id);

  // Loads the |module_list_filter_| using the Module List at |path|.
  void LoadModuleList(const base::FilePath& path);

  // Force the initialization of the IncompatibleApplicationsUpdater and the
  // ModuleBlacklistCacheUpdater instances by triggering an update of the module
  // list component, if needed. Immediately invokes
  // |on_initialization_event_callback| if this instance is already in a final
  // state (Failed to initialize or fully initialized). This is only meant to be
  // used when the chrome://conflicts page is opened by the user.
  enum class State {
    // The initialization failed because the Module List component couldn't be
    // used to initialize the ModuleListFilter.
    kModuleListInvalidFailure,
    // The initialization failed because there was no Module List version
    // available to install.
    kNoModuleListAvailableFailure,
    // The instance is initialized. If their respective feature is enabled, the
    // |incompatible_applications_updater_| & |module_blacklist_cache_updater_|
    // instances are initialized.
    kInitialized,
    // The instance is about to be deleted.
    kDestroyed,
  };
  using OnInitializationCompleteCallback =
      base::OnceCallback<void(State state)>;
  void ForceInitialization(
      OnInitializationCompleteCallback on_initialization_complete_callback);

  // ComponentUpdateService::Observer:
  void OnEvent(Events event, const std::string& component_id) override;

 private:
  // Called when |exe_certificate_info_| finishes its initialization.
  void OnExeCertificateCreated(
      std::unique_ptr<CertificateInfo> exe_certificate_info);

  // Called when |module_list_filter_| finishes its initialization.
  void OnModuleListFilterCreated(
      std::unique_ptr<ModuleListFilter> module_list_filter);

  // Called when |installed_applications_| finishes its initialization.
  void OnInstalledApplicationsCreated(
      std::unique_ptr<InstalledApplications> installed_applications);

  // Initializes either or both |incompatible_applications_updater_| and
  // |module_blacklist_cache_updater_| when the exe_certificate_info_, the
  // module_list_filter_ and the installed_applications_ are available.
  void InitializeIfReady();

  // Checks if the |old_md5_digest| matches the expected one from the Local
  // State file, and updates it to |new_md5_digest|.
  void OnModuleBlacklistCacheUpdated(
      const ModuleBlacklistCacheUpdater::CacheUpdateResult& result);

  // Forcibly triggers an update of the Third Party Module List component. Only
  // invoked when ForceInitialization() is called.
  void ForceModuleListComponentUpdate();

  // Modifies the current state and invokes
  // |on_initialization_complete_callback_|.
  void SetTerminalState(State terminal_state);

  ModuleDatabaseEventSource* const module_database_event_source_;

  scoped_refptr<base::SequencedTaskRunner> background_sequence_;

  // Indicates if the initial Module List has been received. Used to prevent the
  // creation of multiple ModuleListFilter instances.
  bool module_list_received_;

  // Indicates if the OnModuleDatabaseIdle() function has been called once
  // already. Used to prevent the creation of multiple InstalledApplications
  // instances.
  bool on_module_database_idle_called_;

  // The certificate info of the current executable.
  std::unique_ptr<CertificateInfo> exe_certificate_info_;

  // Holds the id of the Third Party Module List component.
  std::string module_list_component_id_;

  // Remembers if ForceInitialization() was invoked.
  bool initialization_forced_;

  // Indicates if an update to the Module List component is needed to initialize
  // the ModuleListFilter.
  bool module_list_update_needed_;

  // Observes the component update service when an update to the Module List
  // component was forced.
  ScopedObserver<component_updater::ComponentUpdateService,
                 component_updater::ComponentUpdateService::Observer>
      component_update_service_observer_;

  // Filters third-party modules against a whitelist and a blacklist.
  std::unique_ptr<ModuleListFilter> module_list_filter_;

  // Retrieves the list of installed applications.
  std::unique_ptr<InstalledApplications> installed_applications_;

  // Maintains the cache of incompatible applications. This member is only
  // initialized when the IncompatibleApplicationsWarning feature is enabled.
  std::unique_ptr<IncompatibleApplicationsUpdater>
      incompatible_applications_updater_;

  // Maintains the module blacklist cache. This member is only initialized when
  // the ThirdPartyModuleBlocking feature is enabled.
  std::unique_ptr<ModuleBlacklistCacheUpdater> module_blacklist_cache_updater_;

  // The final state of this instance.
  base::Optional<State> terminal_state_;

  // The callback that is invoked when |state_| changes.
  OnInitializationCompleteCallback on_initialization_complete_callback_;

  base::WeakPtrFactory<ThirdPartyConflictsManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThirdPartyConflictsManager);
};

#endif  // CHROME_BROWSER_CONFLICTS_THIRD_PARTY_CONFLICTS_MANAGER_WIN_H_
