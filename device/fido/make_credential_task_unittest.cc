// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "device/base/features.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

using TestMakeCredentialTaskCallback =
    ::device::test::StatusAndValueCallbackReceiver<
        CtapDeviceResponseCode,
        base::Optional<AuthenticatorMakeCredentialResponse>>;

}  // namespace

class FidoMakeCredentialTaskTest : public testing::Test {
 public:
  FidoMakeCredentialTaskTest() {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndEnableFeature(kNewCtap2Device);
  }

  std::unique_ptr<MakeCredentialTask> CreateMakeCredentialTask(
      FidoDevice* device) {
    PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
    PublicKeyCredentialUserEntity user(
        fido_parsing_utils::Materialize(test_data::kUserId));
    return std::make_unique<MakeCredentialTask>(
        device,
        CtapMakeCredentialRequest(
            test_data::kClientDataHash, std::move(rp), std::move(user),
            PublicKeyCredentialParams(
                std::vector<PublicKeyCredentialParams::CredentialInfo>(1))),
        AuthenticatorSelectionCriteria(), callback_receiver_.callback());
  }

  std::unique_ptr<MakeCredentialTask>
  CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      FidoDevice* device,
      AuthenticatorSelectionCriteria criteria) {
    PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
    PublicKeyCredentialUserEntity user(
        fido_parsing_utils::Materialize(test_data::kUserId));
    return std::make_unique<MakeCredentialTask>(
        device,
        CtapMakeCredentialRequest(
            test_data::kClientDataHash, std::move(rp), std::move(user),
            PublicKeyCredentialParams(
                std::vector<PublicKeyCredentialParams::CredentialInfo>(1))),
        std::move(criteria), callback_receiver_.callback());
  }

  void RemoveCtapFlag() {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndDisableFeature(kNewCtap2Device);
  }

  TestMakeCredentialTaskCallback& make_credential_callback_receiver() {
    return callback_receiver_;
  }

 protected:
  base::Optional<base::test::ScopedFeatureList> scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestMakeCredentialTaskCallback callback_receiver_;
};

TEST_F(FidoMakeCredentialTaskTest, MakeCredentialSuccess) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
  EXPECT_TRUE(make_credential_callback_receiver().value());
  EXPECT_EQ(device->supported_protocol(), ProtocolVersion::kCtap);
  EXPECT_TRUE(device->device_info());
}

TEST_F(FidoMakeCredentialTaskTest, MakeCredentialWithIncorrectRpIdHash) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponseWithIncorrectRpIdHash);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest, FallbackToU2fRegisterSuccess) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, base::nullopt);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest, TestDefaultU2fRegisterOperationWithoutFlag) {
  RemoveCtapFlag();
  auto device = std::make_unique<MockFidoDevice>();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest, U2fRegisterWithUserVerificationRequired) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, base::nullopt);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
          false /* require_resident_key */,
          UserVerificationRequirement::kRequired));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest, U2fRegisterWithPlatformDeviceRequirement) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, base::nullopt);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kPlatform,
          false /* require_resident_key */,
          UserVerificationRequirement::kPreferred));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest, U2fRegisterWithResidentKeyRequirement) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, base::nullopt);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
          true /* require_resident_key */,
          UserVerificationRequirement::kPreferred));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest,
       UserVerificationAuthenticatorSelectionCriteria) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponseWithoutUvSupport);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
          false /* require_resident_key */,
          UserVerificationRequirement::kRequired));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
  EXPECT_EQ(ProtocolVersion::kCtap, device->supported_protocol());
  EXPECT_TRUE(device->device_info());
  EXPECT_EQ(AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedButNotConfigured,
            device->device_info()->options().user_verification_availability());
}

TEST_F(FidoMakeCredentialTaskTest,
       PlatformDeviceAuthenticatorSelectionCriteria) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponseCrossPlatformDevice);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kPlatform,
          false /* require_resident_key */,
          UserVerificationRequirement::kPreferred));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
  EXPECT_EQ(ProtocolVersion::kCtap, device->supported_protocol());
  EXPECT_TRUE(device->device_info());
  EXPECT_FALSE(device->device_info()->options().is_platform_device());
}

TEST_F(FidoMakeCredentialTaskTest, ResidentKeyAuthenticatorSelectionCriteria) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
          true /* require_resident_key */,
          UserVerificationRequirement::kPreferred));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
  EXPECT_EQ(ProtocolVersion::kCtap, device->supported_protocol());
  EXPECT_TRUE(device->device_info());
  EXPECT_FALSE(device->device_info()->options().supports_resident_key());
}

TEST_F(FidoMakeCredentialTaskTest, SatisfyAllAuthenticatorSelectionCriteria) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kPlatform,
          true /* require_resident_key */,
          UserVerificationRequirement::kRequired));
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
  EXPECT_TRUE(make_credential_callback_receiver().value());
  EXPECT_EQ(ProtocolVersion::kCtap, device->supported_protocol());
  EXPECT_TRUE(device->device_info());
  const auto& device_options = device->device_info()->options();
  EXPECT_TRUE(device_options.is_platform_device());
  EXPECT_TRUE(device_options.supports_resident_key());
  EXPECT_EQ(AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured,
            device_options.user_verification_availability());
}

TEST_F(FidoMakeCredentialTaskTest, IncompatibleUserVerificationSetting) {
  auto device = std::make_unique<MockFidoDevice>();

  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponseWithoutUvSupport);

  const auto task = CreateMakeCredentialTaskWithAuthenticatorSelectionCriteria(
      device.get(),
      AuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
          false /* require_resident_key */,
          UserVerificationRequirement::kRequired));
  make_credential_callback_receiver().WaitForCallback();
  EXPECT_EQ(ProtocolVersion::kCtap, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
  EXPECT_FALSE(make_credential_callback_receiver().value());
}

}  // namespace device
