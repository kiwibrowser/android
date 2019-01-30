// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_REGULAR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_REGULAR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"
#include "chrome/browser/chromeos/login/easy_unlock/short_lived_user_context.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/prefs/pref_change_registrar.h"

namespace base {
class ListValue;
}  // namespace base

namespace cryptauth {
class CryptAuthClient;
class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;
class LocalDeviceDataProvider;
class RemoteDeviceLoader;
class ToggleEasyUnlockResponse;
}  // namespace cryptauth

namespace proximity_auth {
class ProximityAuthProfilePrefManager;
}  // namespace proximity_auth

class Profile;

namespace chromeos {

class EasyUnlockNotificationController;

// EasyUnlockService instance that should be used for regular, non-signin
// profiles.
class EasyUnlockServiceRegular
    : public EasyUnlockService,
      public proximity_auth::ScreenlockBridge::Observer,
      public cryptauth::CryptAuthDeviceManager::Observer,
      public device_sync::DeviceSyncClient::Observer {
 public:
  explicit EasyUnlockServiceRegular(
      Profile* profile,
      device_sync::DeviceSyncClient* device_sync_client);

  // Constructor for tests.
  EasyUnlockServiceRegular(
      Profile* profile,
      std::unique_ptr<EasyUnlockNotificationController> notification_controller,
      device_sync::DeviceSyncClient* device_sync_client);

  ~EasyUnlockServiceRegular() override;

 private:
  // Loads the RemoteDevice instances that will be supplied to
  // ProximityAuthSystem.
  void LoadRemoteDevices();

  // Called when |remote_device_loader_| completes.
  void OnRemoteDevicesLoaded(const cryptauth::RemoteDeviceList& remote_devices);

  void UseLoadedRemoteDevices(
      const cryptauth::RemoteDeviceRefList& remote_devices);

  // EasyUnlockService implementation:
  proximity_auth::ProximityAuthPrefManager* GetProximityAuthPrefManager()
      override;
  EasyUnlockService::Type GetType() const override;
  AccountId GetAccountId() const override;
  void LaunchSetup() override;
  void ClearPermitAccess() override;
  const base::ListValue* GetRemoteDevices() const override;
  void SetRemoteDevices(const base::ListValue& devices) override;
  void RunTurnOffFlow() override;
  void ResetTurnOffFlow() override;
  TurnOffFlowStatus GetTurnOffFlowStatus() const override;
  std::string GetChallenge() const override;
  std::string GetWrappedSecret() const override;
  void RecordEasySignInOutcome(const AccountId& account_id,
                               bool success) const override;
  void RecordPasswordLoginEvent(const AccountId& account_id) const override;
  void InitializeInternal() override;
  void ShutdownInternal() override;
  bool IsAllowedInternal() const override;
  bool IsEnabled() const override;
  bool IsChromeOSLoginEnabled() const override;
  void OnWillFinalizeUnlock(bool success) override;
  void OnSuspendDoneInternal() override;
  void HandleUserReauth(const UserContext& user_context) override;

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override;
  void OnSyncFinished(cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
                      cryptauth::CryptAuthDeviceManager::DeviceChangeResult
                          device_change_result) override;

  // device_sync::DeviceSyncClient::Observer:
  void OnNewDevicesSynced() override;

  void ShowNotificationIfNewDevicePresent(
      const std::set<std::string>& public_keys_before_sync,
      const std::set<std::string>& public_keys_after_sync);

  void OnForceSyncCompleted(bool success);

  // proximity_auth::ScreenlockBridge::Observer implementation:
  void OnScreenDidLock(proximity_auth::ScreenlockBridge::LockHandler::ScreenType
                           screen_type) override;
  void OnScreenDidUnlock(
      proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type)
      override;
  void OnFocusedUserChanged(const AccountId& account_id) override;

  // Sets the new turn-off flow status.
  void SetTurnOffFlowStatus(TurnOffFlowStatus status);

  // Callback for ToggleEasyUnlock CryptAuth API.
  void OnToggleEasyUnlockApiComplete(
      const cryptauth::ToggleEasyUnlockResponse& response);
  void OnToggleEasyUnlockApiFailed(const std::string& error_message);

  void OnTurnOffEasyUnlockCompleted(
      const base::Optional<std::string>& error_code);

  void OnTurnOffEasyUnlockSuccess();
  void OnTurnOffEasyUnlockFailure(const std::string& error_message);

  // Called with the user's credentials (e.g. username and password) after the
  // user reauthenticates to begin setup.
  void OpenSetupAppAfterReauth(const UserContext& user_context);

  // Called after a cryptohome RemoveKey or RefreshKey operation to set the
  // proper hardlock state if the operation is successful.
  void SetHardlockAfterKeyOperation(
      EasyUnlockScreenlockStateHandler::HardlockState state_on_success,
      bool success);

  std::unique_ptr<ShortLivedUserContext> short_lived_user_context_;

  // Returns the CryptAuthEnrollmentManager, which manages the profile's
  // CryptAuth enrollment.
  cryptauth::CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager();

  // Returns the CryptAuthEnrollmentManager, which manages the profile's
  // synced devices from CryptAuth.
  cryptauth::CryptAuthDeviceManager* GetCryptAuthDeviceManager();

  // Refreshes the ChromeOS cryptohome keys if the user has reauthed recently.
  // Otherwise, hardlock the device.
  void RefreshCryptohomeKeysIfPossible();

  cryptauth::RemoteDeviceRefList GetUnlockKeys();

  TurnOffFlowStatus turn_off_flow_status_;
  std::unique_ptr<cryptauth::CryptAuthClient> cryptauth_client_;
  ScopedObserver<cryptauth::CryptAuthDeviceManager, EasyUnlockServiceRegular>
      scoped_crypt_auth_device_manager_observer_;

  // True if the user just unlocked the screen using Easy Unlock. Reset once
  // the screen unlocks. Used to distinguish Easy Unlock-powered unlocks from
  // password-based unlocks for metrics.
  bool will_unlock_using_easy_unlock_;

  // The timestamp for the most recent time when the lock screen was shown. The
  // lock screen is typically shown when the user awakens their computer from
  // sleep -- e.g. by opening the lid -- but can also be shown if the screen is
  // locked but the computer does not go to sleep.
  base::TimeTicks lock_screen_last_shown_timestamp_;

  // Manager responsible for handling the prefs used by proximity_auth classes.
  std::unique_ptr<proximity_auth::ProximityAuthProfilePrefManager>
      pref_manager_;

  // Loads the RemoteDevice instances from CryptAuth and local data.
  std::unique_ptr<cryptauth::RemoteDeviceLoader> remote_device_loader_;

  // Provides local device information from CryptAuth.
  std::unique_ptr<cryptauth::LocalDeviceDataProvider>
      local_device_data_provider_;

  // If a new RemoteDevice was synced while the screen is locked, we defer
  // loading the RemoteDevice until the screen is unlocked. For security,
  // this deferment prevents the lock screen from being changed by a network
  // event.
  bool deferring_device_load_;

  // Responsible for showing all the notifications used for EasyUnlock.
  std::unique_ptr<EasyUnlockNotificationController> notification_controller_;

  device_sync::DeviceSyncClient* device_sync_client_;

  // Stores the unlock keys for EasyUnlock before the current device sync, so we
  // can compare it to the unlock keys after syncing.
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_before_sync_;
  cryptauth::RemoteDeviceRefList remote_device_unlock_keys_before_sync_;

  // True if the pairing changed notification was shown, so that the next time
  // the Chromebook is unlocked, we can show the subsequent 'pairing applied'
  // notification.
  bool shown_pairing_changed_notification_;

  // If this service is the first caller on DeviceSyncClient, it won't have
  // devices cached yet. |is_waiting_for_initial_sync_| is set to true if
  // DeviceSyncClient has no devices, to indicate that we are waiting for the
  // initial sync, to be inspected in OnNewDevicesSynced(). OnNewDevicesSynced()
  // needs to know that it is receiving the initial sync, not a newly forced
  // one, in order to prevent it from running unrelated logic.
  bool is_waiting_for_initial_sync_ = false;

  // Listens to pref changes.
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<EasyUnlockServiceRegular> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegular);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_REGULAR_H_
