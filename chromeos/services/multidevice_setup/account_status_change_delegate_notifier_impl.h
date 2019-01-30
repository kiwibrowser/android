// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/multidevice_setup/account_status_change_delegate_notifier.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace chromeos {

namespace multidevice_setup {

class SetupFlowCompletionRecorder;

// Concrete AccountStatusChangeDelegateNotifier implementation, which uses
// DeviceSyncClient to check for account changes and PrefStore to track previous
// notifications.
class AccountStatusChangeDelegateNotifierImpl
    : public AccountStatusChangeDelegateNotifier,
      public device_sync::DeviceSyncClient::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<AccountStatusChangeDelegateNotifier> BuildInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        PrefService* pref_service,
        SetupFlowCompletionRecorder* setup_flow_completion_recorder,
        base::Clock* clock);

   private:
    static Factory* test_factory_;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~AccountStatusChangeDelegateNotifierImpl() override;

  void SetAccountStatusChangeDelegatePtr(
      mojom::AccountStatusChangeDelegatePtr delegate_ptr);

 private:
  friend class MultiDeviceSetupAccountStatusChangeDelegateNotifierTest;

  static const char kNewUserPotentialHostExistsPrefName[];
  static const char kExistingUserHostSwitchedPrefName[];
  static const char kExistingUserChromebookAddedPrefName[];

  static const char kHostPublicKeyFromMostRecentSyncPrefName[];

  AccountStatusChangeDelegateNotifierImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      PrefService* pref_service,
      SetupFlowCompletionRecorder* setup_flow_completion_recorder,
      base::Clock* clock);

  // AccountStatusChangeDelegateNotifier:
  void OnDelegateSet() override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void CheckForMultiDeviceEvents();

  void CheckForNewUserPotentialHostExistsEvent(
      const cryptauth::RemoteDeviceRefList&);
  void CheckForExistingUserHostSwitchedEvent(
      base::Optional<std::string> host_public_key_before_sync);
  void CheckForExistingUserChromebookAddedEvent(
      bool local_device_was_enabled_client_before_sync);

  // Loads data from previous session using PrefService.
  base::Optional<std::string> LoadHostPublicKeyFromEndOfPreviousSession();

  // Set to base::nullopt if there was no enabled host in the most recent sync.
  base::Optional<std::string> host_public_key_from_most_recent_sync_;
  bool local_device_is_enabled_client_;

  mojom::AccountStatusChangeDelegatePtr delegate_ptr_;
  device_sync::DeviceSyncClient* device_sync_client_;
  PrefService* pref_service_;
  SetupFlowCompletionRecorder* setup_flow_completion_recorder_;
  base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(AccountStatusChangeDelegateNotifierImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_ACCOUNT_STATUS_CHANGE_DELEGATE_NOTIFIER_IMPL_H_
