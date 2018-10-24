// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_metadata.h"

#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace device {
namespace fido {
namespace mac {
namespace {

bool UserEqual(const CredentialMetadata::UserEntity& lhs,
               const CredentialMetadata::UserEntity& rhs) {
  return lhs.id == rhs.id && lhs.name == rhs.name &&
         lhs.display_name == rhs.display_name;
}

base::span<const uint8_t> to_bytes(base::StringPiece in) {
  return base::make_span(reinterpret_cast<const uint8_t*>(in.data()),
                         in.size());
}

class CredentialMetadataTest : public ::testing::Test {
 protected:
  CredentialMetadata::UserEntity DefaultUser() {
    return CredentialMetadata::UserEntity(default_id_, "user", "user@acme.com");
  }

  std::vector<uint8_t> SealCredentialId(CredentialMetadata::UserEntity user) {
    return *CredentialMetadata::SealCredentialId(key_, rp_id_, std::move(user));
  }

  CredentialMetadata::UserEntity UnsealCredentialId(
      base::span<const uint8_t> credential_id) {
    return *CredentialMetadata::UnsealCredentialId(key_, rp_id_, credential_id);
  }

  std::string EncodeRpIdAndUserId(base::StringPiece user_id) {
    return *CredentialMetadata::EncodeRpIdAndUserId(key_, rp_id_,
                                                    to_bytes(user_id));
  }
  std::string EncodeRpId() {
    return *CredentialMetadata::EncodeRpId(key_, rp_id_);
  }

  std::vector<uint8_t> default_id_ = {0, 1, 2, 3};
  std::string rp_id_ = "acme.com";
  std::string key_ = "supersecretsupersecretsupersecre";
  std::string wrong_key_ = "SUPERsecretsupersecretsupersecre";
};

TEST_F(CredentialMetadataTest, CredentialId) {
  std::vector<uint8_t> id = SealCredentialId(DefaultUser());
  EXPECT_EQ(0, (id)[0]);
  EXPECT_EQ(54u, id.size());
  EXPECT_TRUE(UserEqual(UnsealCredentialId(id), DefaultUser()));
}

TEST_F(CredentialMetadataTest, CredentialId_IsRandom) {
  EXPECT_NE(SealCredentialId(DefaultUser()), SealCredentialId(DefaultUser()));
}

TEST_F(CredentialMetadataTest, CredentialId_FailDecrypt) {
  const auto id = SealCredentialId(DefaultUser());
  // Flipping a bit in version, nonce, or ciphertext will fail credential
  // decryption.
  for (size_t i = 0; i < id.size(); i++) {
    std::vector<uint8_t> new_id(id);
    new_id[i] = new_id[i] ^ 0x01;
    EXPECT_FALSE(CredentialMetadata::UnsealCredentialId(key_, rp_id_, new_id));
  }
}

TEST_F(CredentialMetadataTest, CredentialId_InvalidRp) {
  std::vector<uint8_t> id = SealCredentialId(DefaultUser());
  // The credential id is authenticated with the RP, thus decryption under a
  // different RP fails.
  EXPECT_FALSE(CredentialMetadata::UnsealCredentialId(key_, "notacme.com", id));
}

TEST_F(CredentialMetadataTest, EncodeRpIdAndUserId) {
  EXPECT_EQ(64u, EncodeRpIdAndUserId("user@acme.com").size())
      << EncodeRpIdAndUserId("user@acme.com");

  EXPECT_EQ(EncodeRpIdAndUserId("user"), EncodeRpIdAndUserId("user"));
  EXPECT_NE(EncodeRpIdAndUserId("userA"), EncodeRpIdAndUserId("userB"));
  EXPECT_NE(EncodeRpIdAndUserId("user"),
            *CredentialMetadata::EncodeRpIdAndUserId(key_, "notacme.com",
                                                     to_bytes("user")));
  EXPECT_NE(EncodeRpIdAndUserId("user"),
            *CredentialMetadata::EncodeRpIdAndUserId(wrong_key_, rp_id_,
                                                     to_bytes("user")));
}

TEST_F(CredentialMetadataTest, EncodeRpId) {
  EXPECT_EQ(64u, EncodeRpId().size());

  EXPECT_EQ(EncodeRpId(), EncodeRpId());
  EXPECT_NE(EncodeRpId(), *CredentialMetadata::EncodeRpId(key_, "notacme.com"));
  EXPECT_NE(EncodeRpId(), *CredentialMetadata::EncodeRpId(wrong_key_, rp_id_));
}

TEST(CredentialMetadata, GenerateRandomSecret) {
  std::string s1 = CredentialMetadata::GenerateRandomSecret();
  EXPECT_EQ(32u, s1.size());
  std::string s2 = CredentialMetadata::GenerateRandomSecret();
  EXPECT_EQ(32u, s2.size());
  EXPECT_NE(s1, s2);
}

TEST(CredentialMetadata, FromPublicKeyCredentialUserEntity) {
  std::vector<uint8_t> user_id = {{1, 2, 3}};
  PublicKeyCredentialUserEntity in(user_id);
  in.SetUserName("username");
  in.SetDisplayName("display name");
  in.SetIconUrl(GURL("http://rp.foo/user.png"));
  CredentialMetadata::UserEntity out =
      CredentialMetadata::UserEntity::FromPublicKeyCredentialUserEntity(
          std::move(in));
  EXPECT_EQ(user_id, out.id);
  EXPECT_EQ("username", out.name);
  EXPECT_EQ("display name", out.display_name);
}

TEST(CredentialMetadata, ToPublicKeyCredentialUserEntity) {
  std::vector<uint8_t> user_id = {{1, 2, 3}};
  CredentialMetadata::UserEntity in(user_id, "username", "display name");
  PublicKeyCredentialUserEntity out = in.ToPublicKeyCredentialUserEntity();
  EXPECT_EQ(user_id, out.user_id());
  EXPECT_EQ("username", out.user_name().value());
  EXPECT_EQ("display name", out.user_display_name().value());
  EXPECT_FALSE(out.user_icon_url().has_value());
}

}  // namespace
}  // namespace mac
}  // namespace fido
}  // namespace device
