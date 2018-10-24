// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "chromeos/components/proximity_auth/logging/log_buffer.h"
#include "chromeos/components/proximity_auth/messenger_observer.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/cryptauth/connection_observer.h"
#include "components/cryptauth/cryptauth_client.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_gcm_manager.h"
#include "components/cryptauth/remote_device_ref.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

namespace cryptauth {
class ExternalDeviceInfo;
class RemoteDeviceLoader;
}  // namespace cryptauth

namespace proximity_auth {

class RemoteDeviceLifeCycle;
struct RemoteStatusUpdate;

// Handles messages from the chrome://proximity-auth page.
class ProximityAuthWebUIHandler
    : public content::WebUIMessageHandler,
      public LogBuffer::Observer,
      public cryptauth::CryptAuthEnrollmentManager::Observer,
      public cryptauth::CryptAuthDeviceManager::Observer,
      public chromeos::device_sync::DeviceSyncClient::Observer,
      public RemoteDeviceLifeCycle::Observer,
      public MessengerObserver {
 public:
  ProximityAuthWebUIHandler(
      ProximityAuthClient* proximity_auth_client,
      chromeos::device_sync::DeviceSyncClient* device_sync_client);
  ~ProximityAuthWebUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // LogBuffer::Observer:
  void OnLogMessageAdded(const LogBuffer::LogMessage& log_message) override;
  void OnLogBufferCleared() override;

  // CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentStarted() override;
  void OnEnrollmentFinished(bool success) override;

  // CryptAuthDeviceManager::Observer:
  void OnSyncStarted() override;
  void OnSyncFinished(cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
                      cryptauth::CryptAuthDeviceManager::DeviceChangeResult
                          device_change_result) override;

  // chromeos::device_sync::DeviceSyncClient::Observer:
  void OnEnrollmentFinished() override;
  void OnNewDevicesSynced() override;

  // Message handler callbacks.
  void OnWebContentsInitialized(const base::ListValue* args);
  void GetLogMessages(const base::ListValue* args);
  void ClearLogBuffer(const base::ListValue* args);
  void ToggleUnlockKey(const base::ListValue* args);
  void FindEligibleUnlockDevices(const base::ListValue* args);
  void GetLocalState(const base::ListValue* args);
  void ForceEnrollment(const base::ListValue* args);
  void ForceDeviceSync(const base::ListValue* args);
  void ToggleConnection(const base::ListValue* args);

  // Initializes CryptAuth managers, used for development purposes.
  void InitGCMManager();
  void InitEnrollmentManager();
  void InitDeviceManager();

  void OnCryptAuthClientError(const std::string& error_message);
  void OnEasyUnlockToggled(const cryptauth::ToggleEasyUnlockResponse& response);

  void OnFoundEligibleUnlockDevices(
      const cryptauth::FindEligibleUnlockDevicesResponse& response);

  // Called when the RemoteDevice is loaded so we can create a connection.
  void OnRemoteDevicesLoaded(const cryptauth::RemoteDeviceList& remote_devices);

  void StartRemoteDeviceLifeCycle(cryptauth::RemoteDeviceRef remote_device);
  void CleanUpRemoteDeviceLifeCycle();

  std::unique_ptr<base::DictionaryValue> ExternalDeviceInfoToDictionary(
      const cryptauth::ExternalDeviceInfo& device_info);
  std::unique_ptr<base::DictionaryValue> RemoteDeviceToDictionary(
      const cryptauth::RemoteDeviceRef& remote_device);
  std::unique_ptr<base::DictionaryValue> IneligibleDeviceToDictionary(
      const cryptauth::IneligibleDevice& ineligible_device);

  // RemoteDeviceLifeCycle::Observer:
  void OnLifeCycleStateChanged(RemoteDeviceLifeCycle::State old_state,
                               RemoteDeviceLifeCycle::State new_state) override;

  // MessengerObserver:
  void OnRemoteStatusUpdate(const RemoteStatusUpdate& status_update) override;

  void OnForceEnrollmentNow(bool success);
  void OnForceSyncNow(bool success);
  void OnSetSoftwareFeatureState(const std::string public_key,
                                 const base::Optional<std::string>& error_code);
  void OnFindEligibleDevices(const base::Optional<std::string>&,
                             cryptauth::RemoteDeviceRefList eligible_devices,
                             cryptauth::RemoteDeviceRefList ineligible_devices);
  void OnGetDebugInfo(
      chromeos::device_sync::mojom::DebugInfoPtr debug_info_ptr);

  void NotifyOnEnrollmentFinished(
      bool success,
      std::unique_ptr<base::DictionaryValue> enrollment_state);
  void NotifyOnSyncFinished(
      bool was_sync_successful,
      bool changed,
      std::unique_ptr<base::DictionaryValue> device_sync_state);
  void NotifyGotLocalState(
      std::unique_ptr<base::Value> truncated_local_device_id,
      std::unique_ptr<base::DictionaryValue> enrollment_state,
      std::unique_ptr<base::DictionaryValue> device_sync_state,
      std::unique_ptr<base::ListValue> synced_devices);

  std::unique_ptr<base::Value> GetTruncatedLocalDeviceId();

  // These two methods cannot be used if the chromeos::features::kMultiDeviceApi
  // flag is enabled.
  std::unique_ptr<base::DictionaryValue> GetEnrollmentStateDictionary();
  std::unique_ptr<base::DictionaryValue> GetDeviceSyncStateDictionary();

  std::unique_ptr<base::ListValue> GetRemoteDevicesList();

  // The delegate used to fetch dependencies. Must outlive this instance.
  ProximityAuthClient* proximity_auth_client_;
  chromeos::device_sync::DeviceSyncClient* device_sync_client_;
  std::unique_ptr<cryptauth::CryptAuthClientFactory> cryptauth_client_factory_;

  // We only support one concurrent API call.
  std::unique_ptr<cryptauth::CryptAuthClient> cryptauth_client_;

  // True if we get a message from the loaded WebContents to know that it is
  // initialized, and we can inject JavaScript.
  bool web_contents_initialized_;

  // Member variables for connecting to and authenticating the remote device.
  // TODO(tengs): Support multiple simultaenous connections.
  std::unique_ptr<cryptauth::RemoteDeviceLoader> remote_device_loader_;
  base::Optional<cryptauth::RemoteDeviceRef> selected_remote_device_;
  std::unique_ptr<RemoteDeviceLifeCycle> life_cycle_;
  std::unique_ptr<RemoteStatusUpdate> last_remote_status_update_;

  bool enrollment_update_waiting_for_debug_info_ = false;
  bool sync_update_waiting_for_debug_info_ = false;
  bool get_local_state_update_waiting_for_debug_info_ = false;

  base::WeakPtrFactory<ProximityAuthWebUIHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthWebUIHandler);
};

}  // namespace proximity_auth

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_WEBUI_PROXIMITY_AUTH_WEBUI_HANDLER_H_
