// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/webui/proximity_auth_webui_handler.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/components/proximity_auth/messenger.h"
#include "chromeos/components/proximity_auth/remote_device_life_cycle_impl.h"
#include "chromeos/components/proximity_auth/remote_status_update.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_loader.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/secure_context.h"
#include "components/cryptauth/secure_message_delegate_impl.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace proximity_auth {

namespace {

// Keys in the JSON representation of a log message.
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Keys in the JSON representation of a SyncState object for enrollment or
// device sync.
const char kSyncStateLastSuccessTime[] = "lastSuccessTime";
const char kSyncStateNextRefreshTime[] = "nextRefreshTime";
const char kSyncStateRecoveringFromFailure[] = "recoveringFromFailure";
const char kSyncStateOperationInProgress[] = "operationInProgress";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
std::unique_ptr<base::DictionaryValue> LogMessageToDictionary(
    const LogBuffer::LogMessage& log_message) {
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  dictionary->SetString(kLogMessageTextKey, log_message.text);
  dictionary->SetString(
      kLogMessageTimeKey,
      base::TimeFormatTimeOfDayWithMilliseconds(log_message.time));
  dictionary->SetString(kLogMessageFileKey, log_message.file);
  dictionary->SetInteger(kLogMessageLineKey, log_message.line);
  dictionary->SetInteger(kLogMessageSeverityKey,
                         static_cast<int>(log_message.severity));
  return dictionary;
}

// Keys in the JSON representation of an ExternalDeviceInfo proto.
const char kExternalDevicePublicKey[] = "publicKey";
const char kExternalDevicePublicKeyTruncated[] = "publicKeyTruncated";
const char kExternalDeviceFriendlyName[] = "friendlyDeviceName";
const char kExternalDeviceBluetoothAddress[] = "bluetoothAddress";
const char kExternalDeviceUnlockKey[] = "unlockKey";
const char kExternalDeviceMobileHotspot[] = "hasMobileHotspot";
const char kExternalDeviceIsArcPlusPlusEnrollment[] = "isArcPlusPlusEnrollment";
const char kExternalDeviceIsPixelPhone[] = "isPixelPhone";
const char kExternalDeviceConnectionStatus[] = "connectionStatus";
const char kExternalDeviceRemoteState[] = "remoteState";

// The possible values of the |kExternalDeviceConnectionStatus| field.
const char kExternalDeviceConnected[] = "connected";
const char kExternalDeviceDisconnected[] = "disconnected";
const char kExternalDeviceConnecting[] = "connecting";

// Keys in the JSON representation of an IneligibleDevice proto.
const char kIneligibleDeviceReasons[] = "ineligibilityReasons";

// Creates a SyncState JSON object that can be passed to the WebUI.
std::unique_ptr<base::DictionaryValue> CreateSyncStateDictionary(
    double last_success_time,
    double next_refresh_time,
    bool is_recovering_from_failure,
    bool is_enrollment_in_progress) {
  std::unique_ptr<base::DictionaryValue> sync_state(
      new base::DictionaryValue());
  sync_state->SetDouble(kSyncStateLastSuccessTime, last_success_time);
  sync_state->SetDouble(kSyncStateNextRefreshTime, next_refresh_time);
  sync_state->SetBoolean(kSyncStateRecoveringFromFailure,
                         is_recovering_from_failure);
  sync_state->SetBoolean(kSyncStateOperationInProgress,
                         is_enrollment_in_progress);
  return sync_state;
}

}  // namespace

ProximityAuthWebUIHandler::ProximityAuthWebUIHandler(
    ProximityAuthClient* proximity_auth_client,
    chromeos::device_sync::DeviceSyncClient* device_sync_client)
    : proximity_auth_client_(proximity_auth_client),
      device_sync_client_(device_sync_client),
      web_contents_initialized_(false),
      weak_ptr_factory_(this) {
  if (!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    cryptauth_client_factory_ =
        proximity_auth_client_->CreateCryptAuthClientFactory();
  }
}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  LogBuffer::GetInstance()->RemoveObserver(this);

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    device_sync_client_->RemoveObserver(this);
  } else {
    cryptauth::CryptAuthDeviceManager* device_manager =
        proximity_auth_client_->GetCryptAuthDeviceManager();
    if (device_manager)
      device_manager->RemoveObserver(this);
  }
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "onWebContentsInitialized",
      base::BindRepeating(&ProximityAuthWebUIHandler::OnWebContentsInitialized,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "clearLogBuffer",
      base::BindRepeating(&ProximityAuthWebUIHandler::ClearLogBuffer,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&ProximityAuthWebUIHandler::GetLogMessages,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleUnlockKey",
      base::BindRepeating(&ProximityAuthWebUIHandler::ToggleUnlockKey,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "findEligibleUnlockDevices",
      base::BindRepeating(&ProximityAuthWebUIHandler::FindEligibleUnlockDevices,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLocalState",
      base::BindRepeating(&ProximityAuthWebUIHandler::GetLocalState,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceEnrollment",
      base::BindRepeating(&ProximityAuthWebUIHandler::ForceEnrollment,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "forceDeviceSync",
      base::BindRepeating(&ProximityAuthWebUIHandler::ForceDeviceSync,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleConnection",
      base::BindRepeating(&ProximityAuthWebUIHandler::ToggleConnection,
                          base::Unretained(this)));
}

void ProximityAuthWebUIHandler::OnLogMessageAdded(
    const LogBuffer::LogMessage& log_message) {
  std::unique_ptr<base::DictionaryValue> dictionary =
      LogMessageToDictionary(log_message);
  web_ui()->CallJavascriptFunctionUnsafe("LogBufferInterface.onLogMessageAdded",
                                         *dictionary);
}

void ProximityAuthWebUIHandler::OnLogBufferCleared() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "LogBufferInterface.onLogBufferCleared");
}

void ProximityAuthWebUIHandler::OnEnrollmentStarted() {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onEnrollmentStateChanged",
      *GetEnrollmentStateDictionary());
}

void ProximityAuthWebUIHandler::OnEnrollmentFinished(bool success) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
  NotifyOnEnrollmentFinished(success, GetEnrollmentStateDictionary());
}

void ProximityAuthWebUIHandler::OnSyncStarted() {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onDeviceSyncStateChanged",
      *GetDeviceSyncStateDictionary());
}

void ProximityAuthWebUIHandler::OnSyncFinished(
    cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
    cryptauth::CryptAuthDeviceManager::DeviceChangeResult
        device_change_result) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));
  NotifyOnSyncFinished(
      sync_result == cryptauth::CryptAuthDeviceManager::SyncResult::
                         SUCCESS /* was_sync_successful */,
      device_change_result == cryptauth::CryptAuthDeviceManager::
                                  DeviceChangeResult::CHANGED /* changed */,
      GetDeviceSyncStateDictionary());
}

void ProximityAuthWebUIHandler::OnEnrollmentFinished() {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  // OnGetDebugInfo() will call NotifyOnEnrollmentFinished() with the enrollment
  // state info.
  enrollment_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnNewDevicesSynced() {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  // OnGetDebugInfo() will call NotifyOnSyncFinished() with the device sync
  // state info.
  sync_update_waiting_for_debug_info_ = true;
  device_sync_client_->GetDebugInfo(
      base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProximityAuthWebUIHandler::OnWebContentsInitialized(
    const base::ListValue* args) {
  if (!web_contents_initialized_) {
    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
      device_sync_client_->AddObserver(this);
    } else {
      cryptauth::CryptAuthEnrollmentManager* enrollment_manager =
          proximity_auth_client_->GetCryptAuthEnrollmentManager();
      if (enrollment_manager)
        enrollment_manager->AddObserver(this);

      cryptauth::CryptAuthDeviceManager* device_manager =
          proximity_auth_client_->GetCryptAuthDeviceManager();
      if (device_manager)
        device_manager->AddObserver(this);
    }

    LogBuffer::GetInstance()->AddObserver(this);

    web_contents_initialized_ = true;
  }
}

void ProximityAuthWebUIHandler::GetLogMessages(const base::ListValue* args) {
  base::ListValue json_logs;
  for (const auto& log : *LogBuffer::GetInstance()->logs()) {
    json_logs.Append(LogMessageToDictionary(log));
  }
  web_ui()->CallJavascriptFunctionUnsafe("LogBufferInterface.onGotLogMessages",
                                         json_logs);
}

void ProximityAuthWebUIHandler::ClearLogBuffer(const base::ListValue* args) {
  // The OnLogBufferCleared() observer function will be called after the buffer
  // is cleared.
  LogBuffer::GetInstance()->Clear();
}

void ProximityAuthWebUIHandler::ToggleUnlockKey(const base::ListValue* args) {
  std::string public_key_b64, public_key;
  bool make_unlock_key;
  if (args->GetSize() != 2 || !args->GetString(0, &public_key_b64) ||
      !args->GetBoolean(1, &make_unlock_key) ||
      !base::Base64UrlDecode(public_key_b64,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    PA_LOG(ERROR) << "Invalid arguments to toggleUnlockKey";
    return;
  }

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    device_sync_client_->SetSoftwareFeatureState(
        public_key, cryptauth::SoftwareFeature::EASY_UNLOCK_HOST,
        true /* enabled */, true /* is_exclusive */,
        base::BindOnce(&ProximityAuthWebUIHandler::OnSetSoftwareFeatureState,
                       weak_ptr_factory_.GetWeakPtr(), public_key));
  } else {
    cryptauth::ToggleEasyUnlockRequest request;
    request.set_enable(make_unlock_key);
    request.set_public_key(public_key);
    *(request.mutable_device_classifier()) =
        proximity_auth_client_->GetDeviceClassifier();

    PA_LOG(INFO) << "Toggling unlock key:\n"
                 << "    public_key: " << public_key_b64 << "\n"
                 << "    make_unlock_key: " << make_unlock_key;
    cryptauth_client_ = cryptauth_client_factory_->CreateInstance();
    cryptauth_client_->ToggleEasyUnlock(
        request,
        base::Bind(&ProximityAuthWebUIHandler::OnEasyUnlockToggled,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&ProximityAuthWebUIHandler::OnCryptAuthClientError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProximityAuthWebUIHandler::FindEligibleUnlockDevices(
    const base::ListValue* args) {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    device_sync_client_->FindEligibleDevices(
        cryptauth::SoftwareFeature::EASY_UNLOCK_HOST,
        base::BindOnce(&ProximityAuthWebUIHandler::OnFindEligibleDevices,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    cryptauth_client_ = cryptauth_client_factory_->CreateInstance();

    cryptauth::FindEligibleUnlockDevicesRequest request;
    *(request.mutable_device_classifier()) =
        proximity_auth_client_->GetDeviceClassifier();
    cryptauth_client_->FindEligibleUnlockDevices(
        request,
        base::Bind(&ProximityAuthWebUIHandler::OnFoundEligibleUnlockDevices,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&ProximityAuthWebUIHandler::OnCryptAuthClientError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProximityAuthWebUIHandler::ForceEnrollment(const base::ListValue* args) {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    device_sync_client_->ForceEnrollmentNow(
        base::BindOnce(&ProximityAuthWebUIHandler::OnForceEnrollmentNow,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    cryptauth::CryptAuthEnrollmentManager* enrollment_manager =
        proximity_auth_client_->GetCryptAuthEnrollmentManager();
    if (enrollment_manager) {
      enrollment_manager->ForceEnrollmentNow(
          cryptauth::INVOCATION_REASON_MANUAL);
    }
  }
}

void ProximityAuthWebUIHandler::ForceDeviceSync(const base::ListValue* args) {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    device_sync_client_->ForceSyncNow(
        base::BindOnce(&ProximityAuthWebUIHandler::OnForceSyncNow,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    cryptauth::CryptAuthDeviceManager* device_manager =
        proximity_auth_client_->GetCryptAuthDeviceManager();
    if (device_manager)
      device_manager->ForceSyncNow(cryptauth::INVOCATION_REASON_MANUAL);
  }
}

void ProximityAuthWebUIHandler::ToggleConnection(const base::ListValue* args) {
  std::string b64_public_key;
  std::string public_key;
  if (!args->GetSize() || !args->GetString(0, &b64_public_key) ||
      !base::Base64UrlDecode(b64_public_key,
                             base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                             &public_key)) {
    PA_LOG(ERROR) << "Unlock key (" << b64_public_key << ") not found";
    return;
  }

  std::string selected_device_public_key;
  if (selected_remote_device_)
    selected_device_public_key = selected_remote_device_->public_key();

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    for (const auto& remote_device : device_sync_client_->GetSyncedDevices()) {
      if (remote_device.public_key() != public_key)
        continue;

      if (life_cycle_ && selected_device_public_key == public_key) {
        CleanUpRemoteDeviceLifeCycle();
        return;
      }

      StartRemoteDeviceLifeCycle(remote_device);
    }
  } else {
    cryptauth::CryptAuthEnrollmentManager* enrollment_manager =
        proximity_auth_client_->GetCryptAuthEnrollmentManager();
    cryptauth::CryptAuthDeviceManager* device_manager =
        proximity_auth_client_->GetCryptAuthDeviceManager();
    if (!enrollment_manager || !device_manager)
      return;

    for (const auto& unlock_key : device_manager->GetUnlockKeys()) {
      if (unlock_key.public_key() != public_key)
        continue;

      if (life_cycle_ && selected_device_public_key == public_key) {
        CleanUpRemoteDeviceLifeCycle();
        return;
      }

      remote_device_loader_.reset(new cryptauth::RemoteDeviceLoader(
          std::vector<cryptauth::ExternalDeviceInfo>(1, unlock_key),
          proximity_auth_client_->GetAccountId(),
          enrollment_manager->GetUserPrivateKey(),
          cryptauth::SecureMessageDelegateImpl::Factory::NewInstance()));
      remote_device_loader_->Load(
          base::Bind(&ProximityAuthWebUIHandler::OnRemoteDevicesLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }
}

void ProximityAuthWebUIHandler::OnCryptAuthClientError(
    const std::string& error_message) {
  PA_LOG(WARNING) << "CryptAuth request failed: " << error_message;
  base::Value error_string(error_message);
  web_ui()->CallJavascriptFunctionUnsafe("CryptAuthInterface.onError",
                                         error_string);
}

void ProximityAuthWebUIHandler::OnEasyUnlockToggled(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "CryptAuthInterface.onUnlockKeyToggled");
  // TODO(tengs): Update the local state to reflect the toggle.
}

void ProximityAuthWebUIHandler::OnFoundEligibleUnlockDevices(
    const cryptauth::FindEligibleUnlockDevicesResponse& response) {
  base::ListValue eligible_devices;
  for (const auto& external_device : response.eligible_devices()) {
    eligible_devices.Append(ExternalDeviceInfoToDictionary(external_device));
  }

  base::ListValue ineligible_devices;
  for (const auto& ineligible_device : response.ineligible_devices()) {
    ineligible_devices.Append(IneligibleDeviceToDictionary(ineligible_device));
  }

  PA_LOG(INFO) << "Found " << eligible_devices.GetSize()
               << " eligible devices and " << ineligible_devices.GetSize()
               << " ineligible devices.";
  web_ui()->CallJavascriptFunctionUnsafe(
      "CryptAuthInterface.onGotEligibleDevices", eligible_devices,
      ineligible_devices);
}

void ProximityAuthWebUIHandler::GetLocalState(const base::ListValue* args) {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    // OnGetDebugInfo() will call NotifyGotLocalState() with the enrollment and
    // device sync state info.
    get_local_state_update_waiting_for_debug_info_ = true;
    device_sync_client_->GetDebugInfo(
        base::BindOnce(&ProximityAuthWebUIHandler::OnGetDebugInfo,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  NotifyGotLocalState(GetTruncatedLocalDeviceId(),
                      GetEnrollmentStateDictionary(),
                      GetDeviceSyncStateDictionary(), GetRemoteDevicesList());
}

std::unique_ptr<base::Value>
ProximityAuthWebUIHandler::GetTruncatedLocalDeviceId() {
  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    return std::make_unique<base::Value>(
        device_sync_client_->GetLocalDeviceMetadata()
            ->GetTruncatedDeviceIdForLogs());
  }

  std::string local_public_key =
      proximity_auth_client_->GetLocalDevicePublicKey();

  std::string device_id;
  base::Base64UrlEncode(local_public_key,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &device_id);

  return std::make_unique<base::Value>(
      cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(device_id));
}

std::unique_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::GetEnrollmentStateDictionary() {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  cryptauth::CryptAuthEnrollmentManager* enrollment_manager =
      proximity_auth_client_->GetCryptAuthEnrollmentManager();
  if (!enrollment_manager)
    return std::make_unique<base::DictionaryValue>();

  return CreateSyncStateDictionary(
      enrollment_manager->GetLastEnrollmentTime().ToJsTime(),
      enrollment_manager->GetTimeToNextAttempt().InMillisecondsF(),
      enrollment_manager->IsRecoveringFromFailure(),
      enrollment_manager->IsEnrollmentInProgress());
}

std::unique_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::GetDeviceSyncStateDictionary() {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  cryptauth::CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!device_manager)
    return std::make_unique<base::DictionaryValue>();

  return CreateSyncStateDictionary(
      device_manager->GetLastSyncTime().ToJsTime(),
      device_manager->GetTimeToNextAttempt().InMillisecondsF(),
      device_manager->IsRecoveringFromFailure(),
      device_manager->IsSyncInProgress());
}

std::unique_ptr<base::ListValue>
ProximityAuthWebUIHandler::GetRemoteDevicesList() {
  std::unique_ptr<base::ListValue> devices_list_value(new base::ListValue());

  if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    for (const auto& remote_device : device_sync_client_->GetSyncedDevices())
      devices_list_value->Append(RemoteDeviceToDictionary(remote_device));
  } else {
    cryptauth::CryptAuthDeviceManager* device_manager =
        proximity_auth_client_->GetCryptAuthDeviceManager();
    if (!device_manager)
      return devices_list_value;

    for (const auto& unlock_key : device_manager->GetSyncedDevices())
      devices_list_value->Append(ExternalDeviceInfoToDictionary(unlock_key));
  }

  return devices_list_value;
}

void ProximityAuthWebUIHandler::OnRemoteDevicesLoaded(
    const cryptauth::RemoteDeviceList& remote_devices) {
  if (remote_devices.empty()) {
    PA_LOG(WARNING) << "Remote device list is empty.";
    return;
  }

  if (remote_devices[0].persistent_symmetric_key.empty()) {
    PA_LOG(ERROR) << "Failed to derive PSK.";
    return;
  }

  StartRemoteDeviceLifeCycle(cryptauth::RemoteDeviceRef(
      std::make_shared<cryptauth::RemoteDevice>(remote_devices[0])));
}

void ProximityAuthWebUIHandler::StartRemoteDeviceLifeCycle(
    cryptauth::RemoteDeviceRef remote_device) {
  selected_remote_device_ = remote_device;
  life_cycle_.reset(new RemoteDeviceLifeCycleImpl(*selected_remote_device_));
  life_cycle_->AddObserver(this);
  life_cycle_->Start();
}

void ProximityAuthWebUIHandler::CleanUpRemoteDeviceLifeCycle() {
  if (selected_remote_device_) {
    PA_LOG(INFO) << "Cleaning up connection to "
                 << selected_remote_device_->name();
  }
  life_cycle_.reset();
  selected_remote_device_ = base::nullopt;
  last_remote_status_update_.reset();
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onRemoteDevicesChanged", *GetRemoteDevicesList());
}

std::unique_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::ExternalDeviceInfoToDictionary(
    const cryptauth::ExternalDeviceInfo& device_info) {
  DCHECK(!base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  std::string base64_public_key;
  base::Base64UrlEncode(device_info.public_key(),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &base64_public_key);

  // Set the fields in the ExternalDeviceInfo proto.
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  dictionary->SetString(kExternalDevicePublicKey, base64_public_key);
  dictionary->SetString(
      kExternalDevicePublicKeyTruncated,
      cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(base64_public_key));
  dictionary->SetString(kExternalDeviceFriendlyName,
                        device_info.friendly_device_name());
  dictionary->SetString(kExternalDeviceBluetoothAddress,
                        device_info.bluetooth_address());
  dictionary->SetBoolean(kExternalDeviceUnlockKey, device_info.unlock_key());
  dictionary->SetBoolean(kExternalDeviceMobileHotspot,
                         device_info.mobile_hotspot_supported());
  dictionary->SetBoolean(kExternalDeviceIsArcPlusPlusEnrollment,
                         device_info.arc_plus_plus());
  dictionary->SetBoolean(kExternalDeviceIsPixelPhone,
                         device_info.pixel_phone());
  dictionary->SetString(kExternalDeviceConnectionStatus,
                        kExternalDeviceDisconnected);

  cryptauth::CryptAuthDeviceManager* device_manager =
      proximity_auth_client_->GetCryptAuthDeviceManager();
  if (!device_manager)
    return dictionary;

  // If |device_info| is a known unlock key, then combine the proto data with
  // the corresponding local device data (e.g. connection status and remote
  // status updates).
  std::string public_key = device_info.public_key();
  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys =
      device_manager->GetUnlockKeys();
  auto iterator = std::find_if(
      unlock_keys.begin(), unlock_keys.end(),
      [&public_key](const cryptauth::ExternalDeviceInfo& unlock_key) {
        return unlock_key.public_key() == public_key;
      });

  std::string selected_device_public_key;
  if (selected_remote_device_)
    selected_device_public_key = selected_remote_device_->public_key();

  if (iterator == unlock_keys.end() ||
      selected_device_public_key != device_info.public_key())
    return dictionary;

  // Fill in the current Bluetooth connection status.
  std::string connection_status = kExternalDeviceDisconnected;
  if (life_cycle_ &&
      life_cycle_->GetState() ==
          RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    connection_status = kExternalDeviceConnected;
  } else if (life_cycle_) {
    connection_status = kExternalDeviceConnecting;
  }
  dictionary->SetString(kExternalDeviceConnectionStatus, connection_status);

  // Fill the remote status dictionary.
  if (last_remote_status_update_) {
    std::unique_ptr<base::DictionaryValue> status_dictionary(
        new base::DictionaryValue());
    status_dictionary->SetInteger("userPresent",
                                  last_remote_status_update_->user_presence);
    status_dictionary->SetInteger(
        "secureScreenLock",
        last_remote_status_update_->secure_screen_lock_state);
    status_dictionary->SetInteger(
        "trustAgent", last_remote_status_update_->trust_agent_state);
    dictionary->Set(kExternalDeviceRemoteState, std::move(status_dictionary));
  }

  return dictionary;
}

std::unique_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::RemoteDeviceToDictionary(
    const cryptauth::RemoteDeviceRef& remote_device) {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi));

  // Set the fields in the ExternalDeviceInfo proto.
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  dictionary->SetString(kExternalDevicePublicKey, remote_device.GetDeviceId());
  dictionary->SetString(kExternalDevicePublicKeyTruncated,
                        remote_device.GetTruncatedDeviceIdForLogs());
  dictionary->SetString(kExternalDeviceFriendlyName, remote_device.name());
  dictionary->SetBoolean(kExternalDeviceUnlockKey, remote_device.unlock_key());
  dictionary->SetBoolean(kExternalDeviceMobileHotspot,
                         remote_device.supports_mobile_hotspot());
  dictionary->SetString(kExternalDeviceConnectionStatus,
                        kExternalDeviceDisconnected);

  // TODO(crbug.com/852836): Add kExternalDeviceIsArcPlusPlusEnrollment and
  // kExternalDeviceIsPixelPhone values to the dictionary once RemoteDevice
  // carries those relevant fields.

  std::string selected_device_public_key;
  if (selected_remote_device_)
    selected_device_public_key = selected_remote_device_->public_key();

  // If it's the selected remote device, combine the already-populated
  // dictionary with corresponding local device data (e.g. connection status and
  // remote status updates).
  if (selected_device_public_key != remote_device.public_key())
    return dictionary;

  // Fill in the current Bluetooth connection status.
  std::string connection_status = kExternalDeviceDisconnected;
  if (life_cycle_ &&
      life_cycle_->GetState() ==
          RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    connection_status = kExternalDeviceConnected;
  } else if (life_cycle_) {
    connection_status = kExternalDeviceConnecting;
  }
  dictionary->SetString(kExternalDeviceConnectionStatus, connection_status);

  // Fill the remote status dictionary.
  if (last_remote_status_update_) {
    std::unique_ptr<base::DictionaryValue> status_dictionary(
        new base::DictionaryValue());
    status_dictionary->SetInteger("userPresent",
                                  last_remote_status_update_->user_presence);
    status_dictionary->SetInteger(
        "secureScreenLock",
        last_remote_status_update_->secure_screen_lock_state);
    status_dictionary->SetInteger(
        "trustAgent", last_remote_status_update_->trust_agent_state);
    dictionary->Set(kExternalDeviceRemoteState, std::move(status_dictionary));
  }

  return dictionary;
}

std::unique_ptr<base::DictionaryValue>
ProximityAuthWebUIHandler::IneligibleDeviceToDictionary(
    const cryptauth::IneligibleDevice& ineligible_device) {
  std::unique_ptr<base::ListValue> ineligibility_reasons(new base::ListValue());
  for (const auto& reason : ineligible_device.reasons()) {
    ineligibility_reasons->AppendInteger(reason);
  }

  std::unique_ptr<base::DictionaryValue> device_dictionary =
      ExternalDeviceInfoToDictionary(ineligible_device.device());
  device_dictionary->Set(kIneligibleDeviceReasons,
                         std::move(ineligibility_reasons));
  return device_dictionary;
}

void ProximityAuthWebUIHandler::OnLifeCycleStateChanged(
    RemoteDeviceLifeCycle::State old_state,
    RemoteDeviceLifeCycle::State new_state) {
  // Do not re-attempt to find a connection after the first one fails--just
  // abort.
  if ((old_state != RemoteDeviceLifeCycle::State::STOPPED &&
       new_state == RemoteDeviceLifeCycle::State::FINDING_CONNECTION) ||
      new_state == RemoteDeviceLifeCycle::State::AUTHENTICATION_FAILED) {
    // Clean up the life cycle asynchronously, because we are currently in the
    // call stack of |life_cycle_|.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProximityAuthWebUIHandler::CleanUpRemoteDeviceLifeCycle,
                       weak_ptr_factory_.GetWeakPtr()));
  } else if (new_state ==
             RemoteDeviceLifeCycle::State::SECURE_CHANNEL_ESTABLISHED) {
    life_cycle_->GetMessenger()->AddObserver(this);
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onRemoteDevicesChanged", *GetRemoteDevicesList());
}

void ProximityAuthWebUIHandler::OnRemoteStatusUpdate(
    const RemoteStatusUpdate& status_update) {
  PA_LOG(INFO) << "Remote status update:"
               << "\n  user_presence: "
               << static_cast<int>(status_update.user_presence)
               << "\n  secure_screen_lock_state: "
               << static_cast<int>(status_update.secure_screen_lock_state)
               << "\n  trust_agent_state: "
               << static_cast<int>(status_update.trust_agent_state);

  last_remote_status_update_.reset(new RemoteStatusUpdate(status_update));
  std::unique_ptr<base::ListValue> synced_devices = GetRemoteDevicesList();
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onRemoteDevicesChanged", *synced_devices);
}

void ProximityAuthWebUIHandler::OnForceEnrollmentNow(bool success) {
  PA_LOG(INFO) << "Force enrollment result: " << success;
}

void ProximityAuthWebUIHandler::OnForceSyncNow(bool success) {
  PA_LOG(INFO) << "Force sync result: " << success;
}

void ProximityAuthWebUIHandler::OnSetSoftwareFeatureState(
    const std::string public_key,
    const base::Optional<std::string>& error_code) {
  std::string device_id =
      cryptauth::RemoteDeviceRef::GenerateDeviceId(public_key);

  if (error_code) {
    PA_LOG(ERROR) << "Failed to set SoftwareFeature state for device: "
                  << device_id << ", error code: " << *error_code;
  } else {
    PA_LOG(INFO) << "Successfully set SoftwareFeature state for device: "
                 << device_id;
  }
}

void ProximityAuthWebUIHandler::OnFindEligibleDevices(
    const base::Optional<std::string>& error_code,
    cryptauth::RemoteDeviceRefList eligible_devices,
    cryptauth::RemoteDeviceRefList ineligible_devices) {
  if (error_code) {
    PA_LOG(ERROR) << "Failed to find eligible devices: " << *error_code;
    return;
  }

  base::ListValue eligible_devices_list_value;
  for (const auto& device : eligible_devices) {
    eligible_devices_list_value.Append(RemoteDeviceToDictionary(device));
  }

  base::ListValue ineligible_devices_list_value;
  for (const auto& device : ineligible_devices) {
    ineligible_devices_list_value.Append(RemoteDeviceToDictionary(device));
  }

  PA_LOG(INFO) << "Found " << eligible_devices_list_value.GetSize()
               << " eligible devices and "
               << ineligible_devices_list_value.GetSize()
               << " ineligible devices.";
  web_ui()->CallJavascriptFunctionUnsafe(
      "CryptAuthInterface.onGotEligibleDevices", eligible_devices_list_value,
      ineligible_devices_list_value);
}

void ProximityAuthWebUIHandler::OnGetDebugInfo(
    chromeos::device_sync::mojom::DebugInfoPtr debug_info_ptr) {
  if (enrollment_update_waiting_for_debug_info_) {
    enrollment_update_waiting_for_debug_info_ = false;
    NotifyOnEnrollmentFinished(
        true /* success */,
        CreateSyncStateDictionary(
            debug_info_ptr->last_enrollment_time.ToJsTime(),
            debug_info_ptr->time_to_next_enrollment_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_enrollment_failure,
            debug_info_ptr->is_enrollment_in_progress));
  }

  if (sync_update_waiting_for_debug_info_) {
    sync_update_waiting_for_debug_info_ = false;
    NotifyOnSyncFinished(
        true /* was_sync_successful */, true /* changed */,
        CreateSyncStateDictionary(
            debug_info_ptr->last_sync_time.ToJsTime(),
            debug_info_ptr->time_to_next_sync_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_sync_failure,
            debug_info_ptr->is_sync_in_progress));
  }

  if (get_local_state_update_waiting_for_debug_info_) {
    get_local_state_update_waiting_for_debug_info_ = false;
    NotifyGotLocalState(
        GetTruncatedLocalDeviceId(),
        CreateSyncStateDictionary(
            debug_info_ptr->last_enrollment_time.ToJsTime(),
            debug_info_ptr->time_to_next_enrollment_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_enrollment_failure,
            debug_info_ptr->is_enrollment_in_progress),
        CreateSyncStateDictionary(
            debug_info_ptr->last_sync_time.ToJsTime(),
            debug_info_ptr->time_to_next_sync_attempt.InMillisecondsF(),
            debug_info_ptr->is_recovering_from_sync_failure,
            debug_info_ptr->is_sync_in_progress),
        GetRemoteDevicesList());
  }
}

void ProximityAuthWebUIHandler::NotifyOnEnrollmentFinished(
    bool success,
    std::unique_ptr<base::DictionaryValue> enrollment_state) {
  PA_LOG(INFO) << "Enrollment attempt completed with success=" << success
               << ":\n"
               << *enrollment_state;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onEnrollmentStateChanged", *enrollment_state);
}

void ProximityAuthWebUIHandler::NotifyOnSyncFinished(
    bool was_sync_successful,
    bool changed,
    std::unique_ptr<base::DictionaryValue> device_sync_state) {
  PA_LOG(INFO) << "Device sync completed with result=" << was_sync_successful
               << ":\n"
               << *device_sync_state;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onDeviceSyncStateChanged", *device_sync_state);

  if (changed) {
    std::unique_ptr<base::ListValue> synced_devices = GetRemoteDevicesList();
    PA_LOG(INFO) << "New unlock keys obtained after device sync:\n"
                 << *synced_devices;
    web_ui()->CallJavascriptFunctionUnsafe(
        "LocalStateInterface.onRemoteDevicesChanged", *synced_devices);
  }
}

void ProximityAuthWebUIHandler::NotifyGotLocalState(
    std::unique_ptr<base::Value> truncated_local_device_id,
    std::unique_ptr<base::DictionaryValue> enrollment_state,
    std::unique_ptr<base::DictionaryValue> device_sync_state,
    std::unique_ptr<base::ListValue> synced_devices) {
  PA_LOG(INFO) << "==== Got Local State ====\n"
               << "Device ID (truncated): " << *truncated_local_device_id
               << "\nEnrollment State: \n"
               << *enrollment_state << "Device Sync State: \n"
               << *device_sync_state << "Synced devices: \n"
               << *synced_devices;
  web_ui()->CallJavascriptFunctionUnsafe(
      "LocalStateInterface.onGotLocalState", *truncated_local_device_id,
      *enrollment_state, *device_sync_state, *synced_devices);
}

}  // namespace proximity_auth
