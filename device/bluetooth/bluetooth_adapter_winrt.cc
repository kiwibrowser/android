// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_winrt.h"

#include <windows.foundation.collections.h>
#include <windows.foundation.h>
#include <wrl/event.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/win/core_winrt_util.h"
#include "device/bluetooth/bluetooth_device_winrt.h"
#include "device/bluetooth/bluetooth_discovery_filter.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/event_utils_winrt.h"

namespace device {

namespace {

// In order to avoid a name clash with device::BluetoothAdapter we need this
// auxiliary namespace.
namespace uwp {
using ABI::Windows::Devices::Bluetooth::BluetoothAdapter;
}  // namespace uwp
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEAdvertisementWatcherStatus_Aborted;
using ABI::Windows::Devices::Bluetooth::Advertisement::BluetoothLEScanningMode;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    BluetoothLEScanningMode_Active;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisement;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementReceivedEventArgs;
using ABI::Windows::Devices::Bluetooth::Advertisement::
    IBluetoothLEAdvertisementWatcher;
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapter;
using ABI::Windows::Devices::Bluetooth::IBluetoothAdapterStatics;
using ABI::Windows::Devices::Enumeration::DeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformation;
using ABI::Windows::Devices::Enumeration::IDeviceInformationStatics;
using ABI::Windows::Devices::Radios::IRadio;
using ABI::Windows::Devices::Radios::IRadioStatics;
using ABI::Windows::Devices::Radios::Radio;
using ABI::Windows::Devices::Radios::RadioAccessStatus;
using ABI::Windows::Devices::Radios::RadioAccessStatus_Allowed;
using ABI::Windows::Devices::Radios::RadioAccessStatus_DeniedBySystem;
using ABI::Windows::Devices::Radios::RadioAccessStatus_DeniedByUser;
using ABI::Windows::Devices::Radios::RadioAccessStatus_Unspecified;
using ABI::Windows::Devices::Radios::RadioState;
using ABI::Windows::Devices::Radios::RadioState_Off;
using ABI::Windows::Devices::Radios::RadioState_On;
using ABI::Windows::Foundation::Collections::IVector;
using ABI::Windows::Foundation::IAsyncOperation;
using Microsoft::WRL::ComPtr;

bool ResolveCoreWinRT() {
  return base::win::ResolveCoreWinRTDelayload() &&
         base::win::ScopedHString::ResolveCoreWinRTStringDelayload();
}

// Utility functions to pretty print enum values.
constexpr const char* ToCString(RadioAccessStatus access_status) {
  switch (access_status) {
    case RadioAccessStatus_Unspecified:
      return "RadioAccessStatus::Unspecified";
    case RadioAccessStatus_Allowed:
      return "RadioAccessStatus::Allowed";
    case RadioAccessStatus_DeniedByUser:
      return "RadioAccessStatus::DeniedByUser";
    case RadioAccessStatus_DeniedBySystem:
      return "RadioAccessStatus::DeniedBySystem";
  }

  NOTREACHED();
  return "";
}

base::Optional<BluetoothDevice::UUIDList> ExtractAdvertisedUUIDs(
    IBluetoothLEAdvertisement* advertisement) {
  ComPtr<IVector<GUID>> service_uuids;
  HRESULT hr = advertisement->get_ServiceUuids(&service_uuids);
  if (FAILED(hr)) {
    VLOG(2) << "get_ServiceUuids() failed: "
            << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  unsigned num_service_uuids;
  hr = service_uuids->get_Size(&num_service_uuids);
  if (FAILED(hr)) {
    VLOG(2) << "get_Size() failed: " << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  BluetoothDevice::UUIDList advertised_uuids;
  for (size_t i = 0; i < num_service_uuids; ++i) {
    GUID service_uuid;
    hr = service_uuids->GetAt(i, &service_uuid);
    if (FAILED(hr)) {
      VLOG(2) << "GetAt(" << i
              << ") failed: " << logging::SystemErrorCodeToString(hr);
      return base::nullopt;
    }

    advertised_uuids.emplace_back(service_uuid);
  }

  return advertised_uuids;
}

ComPtr<IBluetoothLEAdvertisement> GetAdvertisement(
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  ComPtr<IBluetoothLEAdvertisement> advertisement;
  HRESULT hr = received->get_Advertisement(&advertisement);
  if (FAILED(hr)) {
    VLOG(2) << "get_Advertisement() failed: "
            << logging::SystemErrorCodeToString(hr);
  }

  return advertisement;
}

base::Optional<std::string> GetDeviceName(
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  ComPtr<IBluetoothLEAdvertisement> advertisement = GetAdvertisement(received);
  if (!advertisement)
    return base::nullopt;

  HSTRING local_name;
  HRESULT hr = advertisement->get_LocalName(&local_name);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Local Name failed: "
            << logging::SystemErrorCodeToString(hr);
    return base::nullopt;
  }

  return base::win::ScopedHString(local_name).GetAsUTF8();
}

void ExtractAndUpdateAdvertisementData(
    IBluetoothLEAdvertisementReceivedEventArgs* received,
    BluetoothDevice* device) {
  int16_t rssi;
  HRESULT hr = received->get_RawSignalStrengthInDBm(&rssi);
  if (FAILED(hr)) {
    VLOG(2) << "get_RawSignalStrengthInDBm() failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IBluetoothLEAdvertisement> advertisement = GetAdvertisement(received);
  if (!advertisement)
    return;

  auto advertised_uuids = ExtractAdvertisedUUIDs(advertisement.Get());
  if (!advertised_uuids)
    return;

  // TODO(https://crbug.com/821766): Implement extraction of service data,
  // manufacturer data and tx power.
  device->UpdateAdvertisementData(
      rssi, std::move(*advertised_uuids), BluetoothDevice::ServiceDataMap(),
      BluetoothDevice::ManufacturerDataMap(), nullptr /* tx_power */);
}

}  // namespace

std::string BluetoothAdapterWinrt::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterWinrt::GetName() const {
  return name_;
}

void BluetoothAdapterWinrt::SetName(const std::string& name,
                                    const base::Closure& callback,
                                    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWinrt::IsInitialized() const {
  return is_initialized_;
}

bool BluetoothAdapterWinrt::IsPresent() const {
  // Obtaining the default adapter will fail if no physical adapter is present.
  // Thus a non-zero |adapter| implies that a physical adapter is present.
  return adapter_ != nullptr;
}

bool BluetoothAdapterWinrt::IsPowered() const {
  // Due to an issue on WoW64 we might fail to obtain the radio in OnGetRadio().
  // This is why it can be null here.
  if (!radio_)
    return false;

  RadioState state;
  HRESULT hr = radio_->get_State(&state);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Radio State failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return state == RadioState_On;
}

bool BluetoothAdapterWinrt::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWinrt::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWinrt::IsDiscovering() const {
  NOTIMPLEMENTED();
  return false;
}

BluetoothAdapter::UUIDList BluetoothAdapterWinrt::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterWinrt::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const AdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothLocalGattService* BluetoothAdapterWinrt::GetGattService(
    const std::string& identifier) const {
  NOTIMPLEMENTED();
  return nullptr;
}

BluetoothAdapterWinrt::BluetoothAdapterWinrt() : weak_ptr_factory_(this) {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

BluetoothAdapterWinrt::~BluetoothAdapterWinrt() = default;

void BluetoothAdapterWinrt::Init(InitCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We are wrapping |init_cb| in a ScopedClosureRunner to ensure it gets run no
  // matter how the function exits. Furthermore, we set |is_initialized_| to
  // true if adapter is still active when the callback gets run.
  base::ScopedClosureRunner on_init(base::BindOnce(
      [](base::WeakPtr<BluetoothAdapterWinrt> adapter, InitCallback init_cb) {
        if (adapter)
          adapter->is_initialized_ = true;
        std::move(init_cb).Run();
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(init_cb)));

  if (!ResolveCoreWinRT())
    return;

  ComPtr<IBluetoothAdapterStatics> adapter_statics;
  HRESULT hr = GetBluetoothAdapterStaticsActivationFactory(&adapter_statics);
  if (FAILED(hr)) {
    VLOG(2) << "GetBluetoothAdapterStaticsActivationFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IAsyncOperation<uwp::BluetoothAdapter*>> get_default_adapter_op;
  hr = adapter_statics->GetDefaultAsync(&get_default_adapter_op);
  if (FAILED(hr)) {
    VLOG(2) << "BluetoothAdapter::GetDefaultAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = PostAsyncResults(
      std::move(get_default_adapter_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnGetDefaultAdapter,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

bool BluetoothAdapterWinrt::SetPoweredImpl(bool powered) {
  // Due to an issue on WoW64 we might fail to obtain the radio in OnGetRadio().
  // This is why it can be null here.
  if (!radio_)
    return false;

  const RadioState state = powered ? RadioState_On : RadioState_Off;
  ComPtr<IAsyncOperation<RadioAccessStatus>> set_state_op;
  HRESULT hr = radio_->SetStateAsync(state, &set_state_op);
  if (FAILED(hr)) {
    VLOG(2) << "Radio::SetStateAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  hr = PostAsyncResults(std::move(set_state_op),
                        base::BindOnce(&BluetoothAdapterWinrt::OnSetState,
                                       weak_ptr_factory_.GetWeakPtr()));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
    return false;
  }

  return true;
}

void BluetoothAdapterWinrt::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    DiscoverySessionErrorCallback error_callback) {
  if (num_discovery_sessions_ > 0) {
    ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
    return;
  }

  HRESULT hr = ActivateBluetoothAdvertisementLEWatcherInstance(
      &ble_advertisement_watcher_);
  if (FAILED(hr)) {
    VLOG(2) << "ActivateBluetoothAdvertisementLEWatcherInstance failed: "
            << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  hr = ble_advertisement_watcher_->put_ScanningMode(
      BluetoothLEScanningMode_Active);
  if (FAILED(hr)) {
    VLOG(2) << "Setting ScanningMode to Active failed: "
            << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  auto advertisement_received_token = AddTypedEventHandler(
      ble_advertisement_watcher_.Get(),
      &IBluetoothLEAdvertisementWatcher::add_Received,
      base::BindRepeating(&BluetoothAdapterWinrt::OnAdvertisementReceived,
                          base::Unretained(this)));
  if (!advertisement_received_token) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  advertisement_received_token_ = *advertisement_received_token;

  hr = ble_advertisement_watcher_->Start();
  if (FAILED(hr)) {
    VLOG(2) << "Starting the Advertisement Watcher failed: "
            << logging::SystemErrorCodeToString(hr);
    RemoveAdvertisementReceivedHandler();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  BluetoothLEAdvertisementWatcherStatus watcher_status;
  hr = ble_advertisement_watcher_->get_Status(&watcher_status);
  if (FAILED(hr)) {
    VLOG(2) << "Getting the Watcher Status failed: "
            << logging::SystemErrorCodeToString(hr);
  } else if (watcher_status == BluetoothLEAdvertisementWatcherStatus_Aborted) {
    VLOG(2)
        << "Starting Advertisement Watcher failed, it is in the Aborted state.";
    RemoveAdvertisementReceivedHandler();
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  ++num_discovery_sessions_;
  ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
}

void BluetoothAdapterWinrt::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    DiscoverySessionErrorCallback error_callback) {
  if (num_discovery_sessions_ == 0) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  if (num_discovery_sessions_ > 1) {
    --num_discovery_sessions_;
    ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
    return;
  }

  RemoveAdvertisementReceivedHandler();
  HRESULT hr = ble_advertisement_watcher_->Stop();
  if (FAILED(hr)) {
    VLOG(2) << "Stopped the Advertisement Watcher failed: "
            << logging::SystemErrorCodeToString(hr);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback),
                       UMABluetoothDiscoverySessionOutcome::UNKNOWN));
    return;
  }

  ble_advertisement_watcher_.Reset();
  --num_discovery_sessions_;
  ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
}

void BluetoothAdapterWinrt::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    DiscoverySessionErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWinrt::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
  NOTIMPLEMENTED();
}

HRESULT BluetoothAdapterWinrt::GetBluetoothAdapterStaticsActivationFactory(
    IBluetoothAdapterStatics** statics) const {
  return base::win::GetActivationFactory<
      IBluetoothAdapterStatics,
      RuntimeClass_Windows_Devices_Bluetooth_BluetoothAdapter>(statics);
}

HRESULT BluetoothAdapterWinrt::GetDeviceInformationStaticsActivationFactory(
    IDeviceInformationStatics** statics) const {
  return base::win::GetActivationFactory<
      IDeviceInformationStatics,
      RuntimeClass_Windows_Devices_Enumeration_DeviceInformation>(statics);
}

HRESULT BluetoothAdapterWinrt::GetRadioStaticsActivationFactory(
    IRadioStatics** statics) const {
  return base::win::GetActivationFactory<
      IRadioStatics, RuntimeClass_Windows_Devices_Radios_Radio>(statics);
}

HRESULT
BluetoothAdapterWinrt::ActivateBluetoothAdvertisementLEWatcherInstance(
    IBluetoothLEAdvertisementWatcher** instance) const {
  auto watcher_hstring = base::win::ScopedHString::Create(
      RuntimeClass_Windows_Devices_Bluetooth_Advertisement_BluetoothLEAdvertisementWatcher);
  if (!watcher_hstring.is_valid())
    return E_FAIL;

  ComPtr<IInspectable> inspectable;
  HRESULT hr =
      base::win::RoActivateInstance(watcher_hstring.get(), &inspectable);
  if (FAILED(hr)) {
    VLOG(2) << "RoActivateInstance failed: "
            << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  ComPtr<IBluetoothLEAdvertisementWatcher> watcher;
  hr = inspectable.As(&watcher);
  if (FAILED(hr)) {
    VLOG(2) << "As IBluetoothLEAdvertisementWatcher failed: "
            << logging::SystemErrorCodeToString(hr);
    return hr;
  }

  return watcher.CopyTo(instance);
}

void BluetoothAdapterWinrt::OnGetDefaultAdapter(
    base::ScopedClosureRunner on_init,
    ComPtr<IBluetoothAdapter> adapter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!adapter) {
    VLOG(2) << "Getting Default Adapter failed.";
    return;
  }

  adapter_ = std::move(adapter);
  uint64_t raw_address;
  HRESULT hr = adapter_->get_BluetoothAddress(&raw_address);
  if (FAILED(hr)) {
    VLOG(2) << "Getting BluetoothAddress failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  address_ = BluetoothDevice::CanonicalizeAddress(
      base::StringPrintf("%012llX", raw_address));
  DCHECK(!address_.empty());

  HSTRING device_id;
  hr = adapter_->get_DeviceId(&device_id);
  if (FAILED(hr)) {
    VLOG(2) << "Getting DeviceId failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IDeviceInformationStatics> device_information_statics;
  hr =
      GetDeviceInformationStaticsActivationFactory(&device_information_statics);
  if (FAILED(hr)) {
    VLOG(2) << "GetDeviceInformationStaticsActivationFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IAsyncOperation<DeviceInformation*>> create_from_id_op;
  hr = device_information_statics->CreateFromIdAsync(device_id,
                                                     &create_from_id_op);
  if (FAILED(hr)) {
    VLOG(2) << "CreateFromIdAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = PostAsyncResults(
      std::move(create_from_id_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnCreateFromIdAsync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));
  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnCreateFromIdAsync(
    base::ScopedClosureRunner on_init,
    ComPtr<IDeviceInformation> device_information) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!device_information) {
    VLOG(2) << "Getting Device Information failed.";
    return;
  }

  HSTRING name;
  HRESULT hr = device_information->get_Name(&name);
  if (FAILED(hr)) {
    VLOG(2) << "Getting Name failed: " << logging::SystemErrorCodeToString(hr);
    return;
  }

  name_ = base::win::ScopedHString(name).GetAsUTF8();

  ComPtr<IRadioStatics> radio_statics;
  hr = GetRadioStaticsActivationFactory(&radio_statics);
  if (FAILED(hr)) {
    VLOG(2) << "GetRadioStaticsActivationFactory failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  ComPtr<IAsyncOperation<RadioAccessStatus>> request_access_op;
  hr = radio_statics->RequestAccessAsync(&request_access_op);
  if (FAILED(hr)) {
    VLOG(2) << "RequestAccessAsync failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = PostAsyncResults(
      std::move(request_access_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnRequestAccess,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnRequestAccess(base::ScopedClosureRunner on_init,
                                            RadioAccessStatus access_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (access_status != RadioAccessStatus_Allowed) {
    VLOG(2) << "Got unexpected Radio Access Status: "
            << ToCString(access_status);
    return;
  }

  ComPtr<IAsyncOperation<Radio*>> get_radio_op;
  HRESULT hr = adapter_->GetRadioAsync(&get_radio_op);
  if (FAILED(hr)) {
    VLOG(2) << "GetRadioAsync failed: " << logging::SystemErrorCodeToString(hr);
    return;
  }

  hr = PostAsyncResults(
      std::move(get_radio_op),
      base::BindOnce(&BluetoothAdapterWinrt::OnGetRadio,
                     weak_ptr_factory_.GetWeakPtr(), std::move(on_init)));

  if (FAILED(hr)) {
    VLOG(2) << "PostAsyncResults failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

void BluetoothAdapterWinrt::OnGetRadio(base::ScopedClosureRunner on_init,
                                       ComPtr<IRadio> radio) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!radio) {
    // This happens within WoW64, due to an issue with non-native APIs.
    VLOG(2) << "Getting Radio failed.";
    return;
  }

  radio_ = std::move(radio);
}

void BluetoothAdapterWinrt::OnSetState(RadioAccessStatus access_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (access_status != RadioAccessStatus_Allowed) {
    VLOG(2) << "Got unexpected Radio Access Status: "
            << ToCString(access_status);
  } else {
    NotifyAdapterPoweredChanged(IsPowered());
  }

  DidChangePoweredState();
}

void BluetoothAdapterWinrt::OnAdvertisementReceived(
    IBluetoothLEAdvertisementWatcher* watcher,
    IBluetoothLEAdvertisementReceivedEventArgs* received) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint64_t raw_bluetooth_address;
  HRESULT hr = received->get_BluetoothAddress(&raw_bluetooth_address);
  if (FAILED(hr)) {
    VLOG(2) << "get_BluetoothAddress() failed: "
            << logging::SystemErrorCodeToString(hr);
    return;
  }

  std::string bluetooth_address =
      BluetoothDeviceWinrt::CanonicalizeAddress(raw_bluetooth_address);
  auto it = devices_.find(bluetooth_address);
  const bool is_new_device = (it == devices_.end());
  if (is_new_device) {
    bool was_inserted = false;
    std::tie(it, was_inserted) = devices_.emplace(
        std::move(bluetooth_address),
        std::make_unique<BluetoothDeviceWinrt>(this, raw_bluetooth_address,
                                               GetDeviceName(received)));
    DCHECK(was_inserted);
  }

  BluetoothDevice* const device = it->second.get();
  ExtractAndUpdateAdvertisementData(received, device);

  for (auto& observer : observers_) {
    is_new_device ? observer.DeviceAdded(this, device)
                  : observer.DeviceChanged(this, device);
  }
}

void BluetoothAdapterWinrt::RemoveAdvertisementReceivedHandler() {
  DCHECK(ble_advertisement_watcher_);
  HRESULT hr = ble_advertisement_watcher_->remove_Received(
      advertisement_received_token_);
  if (FAILED(hr)) {
    VLOG(2) << "Removing the Received Handler failed: "
            << logging::SystemErrorCodeToString(hr);
  }
}

}  // namespace device
