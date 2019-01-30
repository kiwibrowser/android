// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_data.h"

#include <utility>

#include "device/fido/attested_credential_data.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {

constexpr size_t kAttestedCredentialDataOffset =
    kRpIdHashLength + kFlagsLength + kSignCounterLength;

}  // namespace

// static
base::Optional<AuthenticatorData> AuthenticatorData::DecodeAuthenticatorData(
    base::span<const uint8_t> auth_data) {
  if (auth_data.size() < kAttestedCredentialDataOffset)
    return base::nullopt;
  auto application_parameter = auth_data.first<kRpIdHashLength>();
  uint8_t flag_byte = auth_data[kRpIdHashLength];
  auto counter =
      auth_data.subspan<kRpIdHashLength + kFlagsLength, kSignCounterLength>();
  auto attested_credential_data =
      AttestedCredentialData::DecodeFromCtapResponse(
          auth_data.subspan(kAttestedCredentialDataOffset));

  return AuthenticatorData(application_parameter, flag_byte, counter,
                           std::move(attested_credential_data));
}

AuthenticatorData::AuthenticatorData(
    base::span<const uint8_t, kRpIdHashLength> application_parameter,
    uint8_t flags,
    base::span<const uint8_t, kSignCounterLength> counter,
    base::Optional<AttestedCredentialData> data)
    : application_parameter_(
          fido_parsing_utils::Materialize(application_parameter)),
      flags_(flags),
      counter_(fido_parsing_utils::Materialize(counter)),
      attested_data_(std::move(data)) {}

AuthenticatorData::AuthenticatorData(AuthenticatorData&& other) = default;
AuthenticatorData& AuthenticatorData::operator=(AuthenticatorData&& other) =
    default;

AuthenticatorData::~AuthenticatorData() = default;

void AuthenticatorData::DeleteDeviceAaguid() {
  if (!attested_data_)
    return;

  attested_data_->DeleteAaguid();
}

std::vector<uint8_t> AuthenticatorData::SerializeToByteArray() const {
  std::vector<uint8_t> authenticator_data;
  fido_parsing_utils::Append(&authenticator_data, application_parameter_);
  authenticator_data.insert(authenticator_data.end(), flags_);
  fido_parsing_utils::Append(&authenticator_data, counter_);
  if (attested_data_) {
    // Attestations are returned in registration responses but not in assertion
    // responses.
    fido_parsing_utils::Append(&authenticator_data,
                               attested_data_->SerializeAsBytes());
  }
  return authenticator_data;
}

std::vector<uint8_t> AuthenticatorData::GetCredentialId() const {
  if (!attested_data_)
    return std::vector<uint8_t>();

  return attested_data_->credential_id();
}

}  // namespace device
