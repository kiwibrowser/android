// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_connection_manager_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/secure_channel/authenticated_channel_impl.h"
#include "chromeos/services/secure_channel/ble_advertiser_impl.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "chromeos/services/secure_channel/ble_initiator_failure_type.h"
#include "chromeos/services/secure_channel/ble_listener_failure_type.h"
#include "chromeos/services/secure_channel/ble_scanner_impl.h"
#include "chromeos/services/secure_channel/ble_synchronizer.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/services/secure_channel/secure_channel_disconnector_impl.h"
#include "components/cryptauth/ble/bluetooth_low_energy_weave_client_connection.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace chromeos {

namespace secure_channel {

namespace {

std::vector<mojom::ConnectionCreationDetail> CreateConnectionDetails(
    ConnectionRole connection_role) {
  std::vector<mojom::ConnectionCreationDetail> creation_details;

  switch (connection_role) {
    case ConnectionRole::kInitiatorRole:
      creation_details.push_back(
          mojom::ConnectionCreationDetail::
              REMOTE_DEVICE_USED_FOREGROUND_BLE_ADVERTISING);
      break;
    case ConnectionRole::kListenerRole:
      creation_details.push_back(
          mojom::ConnectionCreationDetail::
              REMOTE_DEVICE_USED_BACKGROUND_BLE_ADVERTISING);
      break;
  }

  return creation_details;
}

}  // namespace

// static
BleConnectionManagerImpl::Factory*
    BleConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
BleConnectionManagerImpl::Factory* BleConnectionManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleConnectionManagerImpl::Factory::~Factory() = default;

std::unique_ptr<BleConnectionManager>
BleConnectionManagerImpl::Factory::BuildInstance(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    BleServiceDataHelper* ble_service_data_helper,
    TimerFactory* timer_factory) {
  return base::WrapUnique(new BleConnectionManagerImpl(
      bluetooth_adapter, ble_service_data_helper, timer_factory));
}

BleConnectionManagerImpl::BleConnectionManagerImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    BleServiceDataHelper* ble_service_data_helper,
    TimerFactory* timer_factory)
    : bluetooth_adapter_(bluetooth_adapter),
      ble_service_data_helper_(ble_service_data_helper),
      ble_synchronizer_(
          BleSynchronizer::Factory::Get()->BuildInstance(bluetooth_adapter)),
      ble_advertiser_(BleAdvertiserImpl::Factory::Get()->BuildInstance(
          this /* delegate */,
          ble_service_data_helper_,
          ble_synchronizer_.get(),
          timer_factory)),
      ble_scanner_(BleScannerImpl::Factory::Get()->BuildInstance(
          this /* delegate */,
          ble_service_data_helper_,
          ble_synchronizer_.get(),
          bluetooth_adapter)),
      secure_channel_disconnector_(
          SecureChannelDisconnectorImpl::Factory::Get()->BuildInstance()) {}

BleConnectionManagerImpl::~BleConnectionManagerImpl() = default;

void BleConnectionManagerImpl::PerformAttemptBleInitiatorConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  ble_advertiser_->AddAdvertisementRequest(device_id_pair, connection_priority);
  ble_scanner_->AddScanFilter(
      BleScanner::ScanFilter(device_id_pair, ConnectionRole::kInitiatorRole));
}

void BleConnectionManagerImpl::PerformUpdateBleInitiatorConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  ble_advertiser_->UpdateAdvertisementRequestPriority(device_id_pair,
                                                      connection_priority);
}

void BleConnectionManagerImpl::PerformCancelBleInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id())) {
    // Check to see if we are removing the final request for an active channel;
    // if so, that channel needs to be disconnected.
    ProcessPotentialLingeringChannel(device_id_pair.remote_device_id());
    return;
  }

  // If a client canceled its request as a result of being notified of an
  // authenticated channel, that request was not actually active.
  if (notifying_remote_device_id_ == device_id_pair.remote_device_id())
    return;

  ble_advertiser_->RemoveAdvertisementRequest(device_id_pair);
  ble_scanner_->RemoveScanFilter(
      BleScanner::ScanFilter(device_id_pair, ConnectionRole::kInitiatorRole));
}

void BleConnectionManagerImpl::PerformAttemptBleListenerConnection(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id()))
    return;

  ble_scanner_->AddScanFilter(
      BleScanner::ScanFilter(device_id_pair, ConnectionRole::kListenerRole));
}

void BleConnectionManagerImpl::PerformUpdateBleListenerConnectionPriority(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority) {
  // BLE scans are not prioritized, so nothing needs to be done.
}

void BleConnectionManagerImpl::PerformCancelBleListenerConnectionAttempt(
    const DeviceIdPair& device_id_pair) {
  if (DoesAuthenticatingChannelExist(device_id_pair.remote_device_id())) {
    // Check to see if we are removing the final request for an active channel;
    // if so, that channel needs to be disconnected.
    ProcessPotentialLingeringChannel(device_id_pair.remote_device_id());
    return;
  }

  // If a client canceled its request as a result of being notified of an
  // authenticated channel, that request was not actually active.
  if (notifying_remote_device_id_ == device_id_pair.remote_device_id())
    return;

  ble_scanner_->RemoveScanFilter(
      BleScanner::ScanFilter(device_id_pair, ConnectionRole::kListenerRole));
}

void BleConnectionManagerImpl::OnAdvertisingSlotEnded(
    const DeviceIdPair& device_id_pair,
    bool replaced_by_higher_priority_advertisement) {
  NotifyBleInitiatorFailure(
      device_id_pair,
      replaced_by_higher_priority_advertisement
          ? BleInitiatorFailureType::
                kInterruptedByHigherPriorityConnectionAttempt
          : BleInitiatorFailureType::kTimeoutContactingRemoteDevice);
}

void BleConnectionManagerImpl::OnReceivedAdvertisement(
    cryptauth::RemoteDeviceRef remote_device,
    device::BluetoothDevice* bluetooth_device,
    ConnectionRole connection_role) {
  // Create a connection to the device.
  std::unique_ptr<cryptauth::Connection> connection =
      cryptauth::weave::BluetoothLowEnergyWeaveClientConnection::Factory::
          NewInstance(remote_device, bluetooth_adapter_,
                      device::BluetoothUUID(kGattServerUuid), bluetooth_device,
                      false /* should_set_low_connection_latency */);

  SetAuthenticatingChannel(
      remote_device.GetDeviceId(),
      cryptauth::SecureChannel::Factory::NewInstance(std::move(connection)),
      connection_role);
}

void BleConnectionManagerImpl::OnSecureChannelStatusChanged(
    cryptauth::SecureChannel* secure_channel,
    const cryptauth::SecureChannel::Status& old_status,
    const cryptauth::SecureChannel::Status& new_status) {
  std::string remote_device_id =
      GetRemoteDeviceIdForSecureChannel(secure_channel);

  if (new_status == cryptauth::SecureChannel::Status::DISCONNECTED) {
    HandleSecureChannelDisconnection(
        remote_device_id,
        old_status == cryptauth::SecureChannel::Status::AUTHENTICATING
        /* was_authenticating */);
    return;
  }

  if (new_status == cryptauth::SecureChannel::Status::AUTHENTICATED) {
    HandleChannelAuthenticated(remote_device_id);
    return;
  }
}

bool BleConnectionManagerImpl::DoesAuthenticatingChannelExist(
    const std::string& remote_device_id) {
  return base::ContainsKey(remote_device_id_to_secure_channel_map_,
                           remote_device_id);
}

void BleConnectionManagerImpl::SetAuthenticatingChannel(
    const std::string& remote_device_id,
    std::unique_ptr<cryptauth::SecureChannel> secure_channel,
    ConnectionRole connection_role) {
  // Since a channel has been established, all connection attempts to the device
  // should be stopped. Otherwise, it would be possible to pick up additional
  // scan results and try to start a new connection. Multiple simultaneous BLE
  // connections to the same device can interfere with each other.
  PauseConnectionAttemptsToDevice(remote_device_id);

  if (base::ContainsKey(remote_device_id_to_secure_channel_map_,
                        remote_device_id)) {
    PA_LOG(ERROR) << "BleConnectionManager::OnReceivedAdvertisement(): A new "
                  << "channel was created, one already exists for the same "
                  << "remote device ID. ID: "
                  << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                         remote_device_id);
    NOTREACHED();
  }

  cryptauth::SecureChannel* secure_channel_raw = secure_channel.get();

  PA_LOG(INFO) << "BleConnectionManager::OnReceivedAdvertisement(): Connection "
               << "established; starting authentication process. Remote device "
               << "ID: "
               << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                      remote_device_id)
               << ", Connection role: " << connection_role;
  remote_device_id_to_secure_channel_map_[remote_device_id] =
      std::make_pair(std::move(secure_channel), connection_role);

  // Observe the channel to be notified of when either the channel authenticates
  // successfully or faces BLE instability and disconnects.
  secure_channel_raw->AddObserver(this);
  secure_channel_raw->Initialize();
}

void BleConnectionManagerImpl::PauseConnectionAttemptsToDevice(
    const std::string& remote_device_id) {
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    switch (details.connection_role()) {
      case ConnectionRole::kInitiatorRole:
        PerformCancelBleInitiatorConnectionAttempt(details.device_id_pair());
        break;
      case ConnectionRole::kListenerRole:
        PerformCancelBleListenerConnectionAttempt(details.device_id_pair());
        break;
    }
  }
}

void BleConnectionManagerImpl::RestartPausedAttemptsToDevice(
    const std::string& remote_device_id) {
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    ConnectionPriority connection_priority = GetPriorityForAttempt(
        details.device_id_pair(), details.connection_role());

    switch (details.connection_role()) {
      case ConnectionRole::kInitiatorRole:
        PerformAttemptBleInitiatorConnection(details.device_id_pair(),
                                             connection_priority);
        break;
      case ConnectionRole::kListenerRole:
        PerformAttemptBleListenerConnection(details.device_id_pair(),
                                            connection_priority);
        break;
    }
  }
}

void BleConnectionManagerImpl::ProcessPotentialLingeringChannel(
    const std::string& remote_device_id) {
  // If there was no authenticating SecureChannel associated with
  // |remote_device_id|, return early.
  if (!base::ContainsKey(remote_device_id_to_secure_channel_map_,
                         remote_device_id)) {
    return;
  }

  // If there is at least one active request, the channel should remain active.
  if (!GetDetailsForRemoteDevice(remote_device_id).empty())
    return;

  // Extract the map value and remove the entry from the map.
  SecureChannelWithRole channel_with_roll =
      std::move(remote_device_id_to_secure_channel_map_[remote_device_id]);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Disconnect the channel, since it is lingering with no active request.
  PA_LOG(INFO) << "BleConnectionManagerImpl::"
               << "ProcessPotentialLingeringChannel(): Disconnecting lingering "
               << "channel which is no longer associated with any active "
               << "requests. Remote device ID: "
               << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                      remote_device_id);
  channel_with_roll.first->RemoveObserver(this);
  secure_channel_disconnector_->DisconnectSecureChannel(
      std::move(channel_with_roll.first));
}

std::string BleConnectionManagerImpl::GetRemoteDeviceIdForSecureChannel(
    cryptauth::SecureChannel* secure_channel) {
  for (const auto& map_entry : remote_device_id_to_secure_channel_map_) {
    if (map_entry.second.first.get() == secure_channel)
      return map_entry.first;
  }

  PA_LOG(ERROR) << "BleConnectionManager::GetRemoteDeviceIdForSecureChannel(): "
                << "No remote device ID mapped to the provided SecureChannel. ";
  NOTREACHED();
  return std::string();
}

void BleConnectionManagerImpl::HandleSecureChannelDisconnection(
    const std::string& remote_device_id,
    bool was_authenticating) {
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    switch (details.connection_role()) {
      // Initiator role devices are notified of authentication errors as well as
      // GATT instability errors.
      case ConnectionRole::kInitiatorRole:
        NotifyBleInitiatorFailure(
            details.device_id_pair(),
            was_authenticating ? BleInitiatorFailureType::kAuthenticationError
                               : BleInitiatorFailureType::kGattConnectionError);
        break;

      // Listener role devices are only notified of authentication errors.
      case ConnectionRole::kListenerRole:
        if (was_authenticating) {
          NotifyBleListenerFailure(
              details.device_id_pair(),
              BleListenerFailureType::kAuthenticationError);
        }
        break;
    }
  }

  // Stop observer the disconnected channel and remove it from the map.
  remote_device_id_to_secure_channel_map_[remote_device_id]
      .first->RemoveObserver(this);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Since the previous connection failed, the connection attempts that were
  // paused in SetAuthenticatingChannel() need to be started up again. Note
  // that it is possible that clients handled being notified of the GATT failure
  // above by removing the connection request due to too many failures.
  RestartPausedAttemptsToDevice(remote_device_id);
}

void BleConnectionManagerImpl::HandleChannelAuthenticated(
    const std::string& remote_device_id) {
  // Extract the map value and remove the entry from the map.
  SecureChannelWithRole channel_with_role =
      std::move(remote_device_id_to_secure_channel_map_[remote_device_id]);
  remote_device_id_to_secure_channel_map_.erase(remote_device_id);

  // Stop observing the channel; it is about to be passed to a client.
  channel_with_role.first->RemoveObserver(this);

  ConnectionAttemptDetails channel_to_receive =
      ChooseChannelRecipient(remote_device_id, channel_with_role.second);

  // Before notifying clients, set |notifying_remote_device_id_|. This ensure
  // that the PerformCancel*() functions can check to see whether requests need
  // to be removed from BleScanner/BleAdvertiser.
  notifying_remote_device_id_ = remote_device_id;
  NotifyConnectionSuccess(
      channel_to_receive.device_id_pair(), channel_to_receive.connection_role(),
      AuthenticatedChannelImpl::Factory::Get()->BuildInstance(
          CreateConnectionDetails(channel_with_role.second),
          std::move(channel_with_role.first)));
  notifying_remote_device_id_.reset();

  // Restart any attempts which still exist.
  RestartPausedAttemptsToDevice(remote_device_id);
}

ConnectionAttemptDetails BleConnectionManagerImpl::ChooseChannelRecipient(
    const std::string& remote_device_id,
    ConnectionRole connection_role) {
  // More than one connection attempt could correspond to this channel. If so,
  // arbitrarily choose the first one as the recipient of the authenticated
  // channel.
  for (const auto& details : GetDetailsForRemoteDevice(remote_device_id)) {
    // Initiator role corresponds to foreground advertisements.
    if (details.connection_role() == ConnectionRole::kInitiatorRole &&
        connection_role == ConnectionRole::kInitiatorRole) {
      return details;
    }

    // Listener role corresponds to background advertisements.
    if (details.connection_role() == ConnectionRole::kListenerRole &&
        connection_role == ConnectionRole::kListenerRole) {
      return details;
    }
  }

  PA_LOG(ERROR) << "BleConnectionManager::ChooseChannelRecipient(): Could not "
                << "find DeviceIdPair to receive channel. Remote device ID: "
                << cryptauth::RemoteDeviceRef::TruncateDeviceIdForLogs(
                       remote_device_id)
                << ", Role: " << connection_role;
  NOTREACHED();
  return ConnectionAttemptDetails(std::string(), std::string(),
                                  ConnectionMedium::kBluetoothLowEnergy,
                                  ConnectionRole::kInitiatorRole);
}

}  // namespace secure_channel

}  // namespace chromeos
