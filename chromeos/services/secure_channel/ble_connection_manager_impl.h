// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_IMPL_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/services/secure_channel/ble_advertiser.h"
#include "chromeos/services/secure_channel/ble_connection_manager.h"
#include "chromeos/services/secure_channel/ble_scanner.h"
#include "chromeos/services/secure_channel/connection_role.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/services/secure_channel/secure_channel_disconnector.h"
#include "components/cryptauth/secure_channel.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace chromeos {

namespace secure_channel {

class BleServiceDataHelper;
class BleSynchronizerBase;
class SecureChannelDisconnector;
class TimerFactory;

// Concrete BleConnectionManager implementation. This class initializes
// BleAdvertiser and BleScanner objects and utilizes them to bootstrap
// connections. Once a connection is found, BleConnectionManagerImpl creates a
// cryptauth::SecureChannel and waits for it to authenticate successfully. Once
// this process is complete, an AuthenticatedChannel is returned to the client.
class BleConnectionManagerImpl : public BleConnectionManager,
                                 public BleAdvertiser::Delegate,
                                 public BleScanner::Delegate,
                                 public cryptauth::SecureChannel::Observer {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<BleConnectionManager> BuildInstance(
        scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
        BleServiceDataHelper* ble_service_data_helper,
        TimerFactory* timer_factory);

   private:
    static Factory* test_factory_;
  };

  ~BleConnectionManagerImpl() override;

 private:
  BleConnectionManagerImpl(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
      BleServiceDataHelper* ble_service_data_helper,
      TimerFactory* timer_factory);

  // BleConnectionManager:
  void PerformAttemptBleInitiatorConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleInitiatorConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
  void PerformAttemptBleListenerConnection(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformUpdateBleListenerConnectionPriority(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority) override;
  void PerformCancelBleListenerConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;

  // BleAdvertiser::Delegate:
  void OnAdvertisingSlotEnded(
      const DeviceIdPair& device_id_pair,
      bool replaced_by_higher_priority_advertisement) override;

  // BleScanner::Delegate:
  void OnReceivedAdvertisement(cryptauth::RemoteDeviceRef remote_device,
                               device::BluetoothDevice* bluetooth_device,
                               ConnectionRole connection_role) override;

  // cryptauth::SecureChannel::Observer:
  void OnSecureChannelStatusChanged(
      cryptauth::SecureChannel* secure_channel,
      const cryptauth::SecureChannel::Status& old_status,
      const cryptauth::SecureChannel::Status& new_status) override;

  // Returns whether a channel exists connecting to |remote_device_id|,
  // regardless of the local device ID or the role used to create the
  // connection.
  bool DoesAuthenticatingChannelExist(const std::string& remote_device_id);

  // Adds |secure_channel| to |remote_device_id_to_secure_channel_map_| and
  // pauses any ongoing attempts to |remote_device_id|, since a connection has
  // already been established to that device.
  void SetAuthenticatingChannel(
      const std::string& remote_device_id,
      std::unique_ptr<cryptauth::SecureChannel> secure_channel,
      ConnectionRole connection_role);

  // Pauses pending connection attempts (scanning and/or advertising) to
  // |remote_device_id|.
  void PauseConnectionAttemptsToDevice(const std::string& remote_device_id);

  // Restarts connections which were paused as part of
  // PauseConnectionAttemptsToDevice();
  void RestartPausedAttemptsToDevice(const std::string& remote_device_id);

  // Checks to see if there is a leftover channel authenticating with
  // |remote_device_id| even though there are no pending requests for a
  // connection to that device. This situation arises when an active request is
  // canceled after a connection has been established but before that connection
  // has been fully authenticated. This function disconnects the channel in the
  // case that it finds one.
  void ProcessPotentialLingeringChannel(const std::string& remote_device_id);

  std::string GetRemoteDeviceIdForSecureChannel(
      cryptauth::SecureChannel* secure_channel);
  void HandleSecureChannelDisconnection(const std::string& remote_device_id,
                                        bool was_authenticating);
  void HandleChannelAuthenticated(const std::string& remote_device_id);

  // Chooses the connection attempt which will receive the success callback.
  // It is possible that there is more than one possible recipient in the case
  // that two attempts are made with the same remote device ID and connection
  // role but different local device IDs. In the case of multiple possible
  // recipients, we arbitrarily choose the one which was registered first.
  ConnectionAttemptDetails ChooseChannelRecipient(
      const std::string& remote_device_id,
      ConnectionRole connection_role);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  BleServiceDataHelper* ble_service_data_helper_;

  std::unique_ptr<BleSynchronizerBase> ble_synchronizer_;
  std::unique_ptr<BleAdvertiser> ble_advertiser_;
  std::unique_ptr<BleScanner> ble_scanner_;
  std::unique_ptr<SecureChannelDisconnector> secure_channel_disconnector_;

  using SecureChannelWithRole =
      std::pair<std::unique_ptr<cryptauth::SecureChannel>, ConnectionRole>;
  base::flat_map<std::string, SecureChannelWithRole>
      remote_device_id_to_secure_channel_map_;
  base::Optional<std::string> notifying_remote_device_id_;

  DISALLOW_COPY_AND_ASSIGN(BleConnectionManagerImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_CONNECTION_MANAGER_IMPL_H_
