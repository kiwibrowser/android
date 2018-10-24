// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_register_operation.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/apdu/apdu_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/u2f_command_constructor.h"

namespace device {

U2fRegisterOperation::U2fRegisterOperation(
    FidoDevice* device,
    const CtapMakeCredentialRequest& request,
    DeviceResponseCallback callback)
    : DeviceOperation(device, request, std::move(callback)),
      weak_factory_(this) {}

U2fRegisterOperation::~U2fRegisterOperation() = default;

void U2fRegisterOperation::Start() {
  DCHECK(IsConvertibleToU2fRegisterCommand(request()));

  const auto& exclude_list = request().exclude_list();
  if (!exclude_list || exclude_list->empty()) {
    TryRegistration(false /* is_duplicate_registration */);
  } else {
    auto it = request().exclude_list()->cbegin();
    DispatchDeviceRequest(
        ConvertToU2fCheckOnlySignCommand(request(), *it),
        base::BindOnce(&U2fRegisterOperation::OnCheckForExcludedKeyHandle,
                       weak_factory_.GetWeakPtr(), it));
  }
}

void U2fRegisterOperation::TryRegistration(bool is_duplicate_registration) {
  auto command = is_duplicate_registration
                     ? ConstructBogusU2fRegistrationCommand()
                     : ConvertToU2fRegisterCommand(request());

  DispatchDeviceRequest(
      std::move(command),
      base::BindOnce(&U2fRegisterOperation::OnRegisterResponseReceived,
                     weak_factory_.GetWeakPtr(), is_duplicate_registration));
}

void U2fRegisterOperation::OnRegisterResponseReceived(
    bool is_duplicate_registration,
    base::Optional<std::vector<uint8_t>> device_response) {
  const auto& apdu_response =
      device_response
          ? apdu::ApduResponse::CreateFromMessage(std::move(*device_response))
          : base::nullopt;
  auto return_code = apdu_response ? apdu_response->status()
                                   : apdu::ApduResponse::Status::SW_WRONG_DATA;

  switch (return_code) {
    case apdu::ApduResponse::Status::SW_NO_ERROR: {
      if (is_duplicate_registration) {
        std::move(callback())
            .Run(CtapDeviceResponseCode::kCtap2ErrCredentialExcluded,
                 base::nullopt);
        break;
      }

      auto response =
          AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
              fido_parsing_utils::CreateSHA256Hash(request().rp().rp_id()),
              apdu_response->data());
      std::move(callback())
          .Run(CtapDeviceResponseCode::kSuccess, std::move(response));
      break;
    }
    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED:
      // Waiting for user touch, retry after delay.
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&U2fRegisterOperation::TryRegistration,
                         weak_factory_.GetWeakPtr(), is_duplicate_registration),
          kU2fRetryDelay);

      break;
    default:
      // An error has occurred, quit trying this device.
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      break;
  }
}

void U2fRegisterOperation::OnCheckForExcludedKeyHandle(
    ExcludeListIterator it,
    base::Optional<std::vector<uint8_t>> device_response) {
  const auto& apdu_response =
      device_response
          ? apdu::ApduResponse::CreateFromMessage(std::move(*device_response))
          : base::nullopt;
  auto return_code = apdu_response ? apdu_response->status()
                                   : apdu::ApduResponse::Status::SW_WRONG_DATA;
  switch (return_code) {
    case apdu::ApduResponse::Status::SW_NO_ERROR:
    case apdu::ApduResponse::Status::SW_CONDITIONS_NOT_SATISFIED: {
      // Duplicate registration found. Call bogus registration to check for
      // user presence (touch) and terminate the registration process.
      DispatchDeviceRequest(
          ConstructBogusU2fRegistrationCommand(),
          base::BindOnce(&U2fRegisterOperation::OnRegisterResponseReceived,
                         weak_factory_.GetWeakPtr(),
                         true /* is_duplicate_registration */));
      break;
    }

    case apdu::ApduResponse::Status::SW_WRONG_DATA:
      // Continue to iterate through the provided key handles in the exclude
      // list and check for already registered keys.
      if (++it != request().exclude_list()->cend()) {
        DispatchDeviceRequest(
            ConvertToU2fCheckOnlySignCommand(request(), *it),
            base::BindOnce(&U2fRegisterOperation::OnCheckForExcludedKeyHandle,
                           weak_factory_.GetWeakPtr(), it));
      } else {
        // Reached the end of exclude list with no duplicate credential.
        // Proceed with registration.
        DispatchDeviceRequest(
            ConvertToU2fRegisterCommand(request()),
            base::BindOnce(&U2fRegisterOperation::OnRegisterResponseReceived,
                           weak_factory_.GetWeakPtr(),
                           false /* is_duplicate_registration */));
      }
      break;
    default:
      // Some sort of failure occurred. Silently drop device request.
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      break;
  }
}

}  // namespace device
