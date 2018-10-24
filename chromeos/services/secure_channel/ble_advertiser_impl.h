// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_ADVERTISER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_ADVERTISER_IMPL_H_

#include <array>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/secure_channel/ble_advertiser.h"
#include "chromeos/services/secure_channel/ble_constants.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_priority.h"

namespace base {
class Timer;
}  // namespace base

namespace chromeos {

namespace secure_channel {

class BleServiceDataHelper;
class BleSynchronizerBase;
class ErrorTolerantBleAdvertisement;
class SharedResourceScheduler;
class TimerFactory;

// Concrete BleAdvertiser implementation. Because systems have a limited number
// of BLE advertisement slots, this class limits the number of concurrent
// advertisements to kMaxConcurrentAdvertisements.
//
// This class tracks two types of requests: active requests (i.e., ones who are
// scheduled to be advertising) and queued requests (i.e., ones who are waiting
// for their turn to use a BLE advertisement slot). A request with a higher
// priority is always given an active advertising slot before a class with a
// lower priority. For equal priorities, a round-robin algorithm is used.
//
// An active advertisement remains active until it is removed by the client,
// pre-empted by another request with a higher priority, or until its timeslot
// ends. This class provides kNumSecondsPerAdvertisementTimeslot seconds for
// each timeslot. When a timeslot ends or when a request is replaced by a
// higher-priority request, the delegate is notified. The delegate is not
// notified when a device is explicitly removed.
class BleAdvertiserImpl : public BleAdvertiser {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<BleAdvertiser> BuildInstance(
        Delegate* delegate,
        BleServiceDataHelper* ble_service_data_helper,
        BleSynchronizerBase* ble_synchronizer_base,
        TimerFactory* timer_factory);

   private:
    static Factory* test_factory_;
  };

  ~BleAdvertiserImpl() override;

 private:
  friend class SecureChannelBleAdvertiserImplTest;

  struct ActiveAdvertisementRequest {
    ActiveAdvertisementRequest(DeviceIdPair device_id_pair,
                               ConnectionPriority connection_priority,
                               std::unique_ptr<base::Timer> timer);
    virtual ~ActiveAdvertisementRequest();

    DeviceIdPair device_id_pair;
    ConnectionPriority connection_priority;
    std::unique_ptr<base::Timer> timer;

    DISALLOW_COPY_AND_ASSIGN(ActiveAdvertisementRequest);
  };

  static const int64_t kNumSecondsPerAdvertisementTimeslot;

  BleAdvertiserImpl(Delegate* delegate,
                    BleServiceDataHelper* ble_service_data_helper,
                    BleSynchronizerBase* ble_synchronizer_base,
                    TimerFactory* timer_factory);

  // BleAdvertiser:
  void AddAdvertisementRequest(const DeviceIdPair& request,
                               ConnectionPriority connection_priority) override;
  void UpdateAdvertisementRequestPriority(
      const DeviceIdPair& request,
      ConnectionPriority connection_priority) override;
  void RemoveAdvertisementRequest(const DeviceIdPair& request) override;

  bool ReplaceLowPriorityAdvertisementIfPossible(
      ConnectionPriority connection_priority);
  base::Optional<size_t> GetIndexWithLowerPriority(
      ConnectionPriority connection_priority);
  void UpdateAdvertisementState();
  void AddActiveAdvertisementRequest(size_t index_to_add);
  void AddActiveAdvertisement(size_t index_to_add);
  base::Optional<size_t> GetIndexForActiveRequest(const DeviceIdPair& request);
  void StopAdvertisementRequestAndUpdateActiveRequests(
      size_t index,
      bool replaced_by_higher_priority_advertisement,
      bool should_reschedule);
  void StopActiveAdvertisement(size_t index);
  void OnActiveAdvertisementStopped(size_t index);

  BleServiceDataHelper* ble_service_data_helper_;
  BleSynchronizerBase* ble_synchronizer_base_;
  TimerFactory* timer_factory_;

  std::unique_ptr<SharedResourceScheduler> shared_resource_scheduler_;
  DeviceIdPairSet all_requests_;

  // An array of length kMaxConcurrentAdvertisements, whose elements correspond
  // to the active BLE advertisement requests. Elements in this array are
  // scheduled to be advertising at this time. However, because stopping
  // advertisements is an asynchronous operation, the active requests may not
  // necessarily correspond to the active advertisements.
  std::array<std::unique_ptr<ActiveAdvertisementRequest>,
             kMaxConcurrentAdvertisements>
      active_advertisement_requests_;

  // An array of length kMaxConcurrentAdvertisements whose elements correspond
  // to the active BLE advertisements.
  std::array<std::unique_ptr<ErrorTolerantBleAdvertisement>,
             kMaxConcurrentAdvertisements>
      active_advertisements_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertiserImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_BLE_ADVERTISER_IMPL_H_
