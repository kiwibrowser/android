// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_task.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "device/base/features.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/ctap_empty_authenticator_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/u2f_command_constructor.h"
#include "device/fido/u2f_sign_operation.h"

namespace device {

namespace {

bool ResponseContainsUserIdentifiableInfo(
    const AuthenticatorGetAssertionResponse& response) {
  const auto& user_entity = response.user_entity();
  if (!user_entity)
    return false;

  return user_entity->user_display_name() || user_entity->user_name() ||
         user_entity->user_icon_url();
}

}  // namespace

GetAssertionTask::GetAssertionTask(FidoDevice* device,
                                   CtapGetAssertionRequest request,
                                   GetAssertionTaskCallback callback)
    : FidoTask(device),
      request_(std::move(request)),
      callback_(std::move(callback)),
      weak_factory_(this) {}

GetAssertionTask::~GetAssertionTask() = default;

void GetAssertionTask::StartTask() {
  if (base::FeatureList::IsEnabled(kNewCtap2Device)) {
    GetAuthenticatorInfo(
        base::BindOnce(&GetAssertionTask::GetAssertion,
                       weak_factory_.GetWeakPtr()),
        base::BindOnce(&GetAssertionTask::U2fSign, weak_factory_.GetWeakPtr()));
  } else {
    U2fSign();
  }
}

void GetAssertionTask::GetAssertion() {
  if (!CheckUserVerificationCompatible()) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  sign_operation_ =
      std::make_unique<Ctap2DeviceOperation<CtapGetAssertionRequest,
                                            AuthenticatorGetAssertionResponse>>(
          device(), request_,
          base::BindOnce(&GetAssertionTask::OnCtapGetAssertionResponseReceived,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&ReadCTAPGetAssertionResponse));
  sign_operation_->Start();
}

void GetAssertionTask::U2fSign() {
  device()->set_supported_protocol(ProtocolVersion::kU2f);
  if (!IsConvertibleToU2fSignCommand(request_)) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  sign_operation_ = std::make_unique<U2fSignOperation>(
      device(), request_,
      base::BindOnce(&GetAssertionTask::OnCtapGetAssertionResponseReceived,
                     weak_factory_.GetWeakPtr()));
  sign_operation_->Start();
}

bool GetAssertionTask::CheckRequirementsOnReturnedUserEntities(
    const AuthenticatorGetAssertionResponse& response) {
  // If assertion has been made without user verification, user identifiable
  // information must not be included.
  if (!response.auth_data().obtained_user_verification() &&
      ResponseContainsUserIdentifiableInfo(response)) {
    return false;
  }

  // For resident key credentials, user id of the user entity is mandatory.
  if ((!request_.allow_list() || request_.allow_list()->empty()) &&
      !response.user_entity()) {
    return false;
  }

  // When multiple accounts exist for specified RP ID, user entity is mandatory.
  if (response.num_credentials().value_or(0u) > 1 && !response.user_entity()) {
    return false;
  }

  return true;
}

bool GetAssertionTask::CheckRequirementsOnReturnedCredentialId(
    const AuthenticatorGetAssertionResponse& response) {
  if (device()->device_info() &&
      device()->device_info()->options().supports_resident_key()) {
    return true;
  }

  const auto& allow_list = request_.allow_list();
  return allow_list &&
         (allow_list->size() == 1 ||
          std::any_of(allow_list->cbegin(), allow_list->cend(),
                      [&response](const auto& credential) {
                        return credential.id() == response.raw_credential_id();
                      }));
}

void GetAssertionTask::OnCtapGetAssertionResponseReceived(
    CtapDeviceResponseCode response_code,
    base::Optional<AuthenticatorGetAssertionResponse> device_response) {
  if (response_code != CtapDeviceResponseCode::kSuccess) {
    std::move(callback_).Run(response_code, base::nullopt);
    return;
  }

  if (!device_response || !device_response->CheckRpIdHash(request_.rp_id()) ||
      !CheckRequirementsOnReturnedCredentialId(*device_response) ||
      !CheckRequirementsOnReturnedUserEntities(*device_response)) {
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  std::move(callback_).Run(response_code, std::move(device_response));
}

bool GetAssertionTask::CheckUserVerificationCompatible() {
  if (!device()->device_info()) {
    return request_.user_verification() !=
           UserVerificationRequirement::kRequired;
  }

  const auto uv_availability =
      device()->device_info()->options().user_verification_availability();

  switch (request_.user_verification()) {
    case UserVerificationRequirement::kRequired:
      return uv_availability ==
             AuthenticatorSupportedOptions::UserVerificationAvailability::
                 kSupportedAndConfigured;

    case UserVerificationRequirement::kDiscouraged:
      return true;

    case UserVerificationRequirement::kPreferred:
      if (uv_availability ==
          AuthenticatorSupportedOptions::UserVerificationAvailability::
              kSupportedAndConfigured) {
        request_.SetUserVerification(UserVerificationRequirement::kRequired);
      } else {
        request_.SetUserVerification(UserVerificationRequirement::kDiscouraged);
      }
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace device
