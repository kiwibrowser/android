// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
#define DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "crypto/aead.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"

namespace device {

class PublicKeyCredentialUserEntity;

namespace fido {
namespace mac {

// CredentialMetadata generates credential IDs from the associated user entity
// (user ID, name and display name) by encrypting them under a key tied to the
// current Chrome profile. This gives us separation of credentials per Chrome
// profile. It also guarantees that account metadata in the OS keychain is
// rendered unusable after the Chrome profile and the associated encryption key
// have been deleted, in order to limit leakage of account metadata, such as
// the list of RPs with registered credentials, into the OS keychain.
//
// Credential IDs have following format
//    | version  |    nonce   | AEAD(pt=CBOR(user_entity), |
//    | (1 byte) | (12 bytes) |      nonce=nonce,          |
//    |          |            |      ad=(version, rpID))   |
// with version as 0x00, a random 12-byte nonce, and using AES-256-GCM as the
// AEAD.
//
// CredentialMetadata also encodes the user ID and RP ID for storage in the OS
// keychain by computing their HMAC.
//
// TODO(martinkr): We currently do not store profile icon URLs.
class COMPONENT_EXPORT(DEVICE_FIDO) CredentialMetadata {
 public:
  // Generate a new random secret to use with the public interface of
  // CredentialMetadata. Chrome stores this secret in the Profile Prefs.
  static std::string GenerateRandomSecret();

  // UserEntity loosely corresponds to a PublicKeyCredentialUserEntity
  // (https://www.w3.org/TR/webauthn/#sctn-user-credential-params). Values of
  // this type should be moved whenever possible.
  struct UserEntity {
   public:
    static UserEntity FromPublicKeyCredentialUserEntity(
        const PublicKeyCredentialUserEntity&);

    UserEntity(std::vector<uint8_t> id_,
               std::string name_,
               std::string display_);
    UserEntity(const UserEntity&);
    UserEntity(UserEntity&&);
    UserEntity& operator=(UserEntity&&);
    ~UserEntity();

    PublicKeyCredentialUserEntity ToPublicKeyCredentialUserEntity();

    std::vector<uint8_t> id;
    std::string name;
    std::string display_name;
  };

  // SealCredentialId encrypts the given UserEntity into a credential id.
  static base::Optional<std::vector<uint8_t>> SealCredentialId(
      const std::string& secret,
      const std::string& rp_id,
      const UserEntity& user);

  // UnsealCredentialId attempts to decrypt a UserEntity from a given credential
  // id.
  static base::Optional<UserEntity> UnsealCredentialId(
      const std::string& secret,
      const std::string& rp_id,
      base::span<const uint8_t> credential_id);

  // EncodeRpIdAndUserId encodes the concatenation of RP ID and user ID for
  // storage in the macOS keychain.
  static base::Optional<std::string> EncodeRpIdAndUserId(
      const std::string& secret,
      const std::string& rp_id,
      base::span<const uint8_t> user_id);

  // EncodeRpId encodes the given RP ID for storage in the macOS keychain.
  static base::Optional<std::string> EncodeRpId(const std::string& secret,
                                                const std::string& rp_id);

 private:
  static constexpr uint8_t kVersion = 0x00;

  // MakeAad returns the concatenation of |kVersion| and |rp_id|,
  // which is used as the additional authenticated data (AAD) input to the AEAD.
  static std::string MakeAad(const std::string& rp_id);

  CredentialMetadata(const std::string& secret);
  ~CredentialMetadata();

  base::Optional<std::string> Seal(base::span<const uint8_t> nonce,
                                   base::span<const uint8_t> plaintext,
                                   base::StringPiece authenticated_data) const;
  base::Optional<std::string> Unseal(
      base::span<const uint8_t> nonce,
      base::span<const uint8_t> ciphertext,
      base::StringPiece authenticated_data) const;
  base::Optional<std::string> HmacForStorage(base::StringPiece data) const;

  // Used to derive keys for the HMAC and AEAD operations. Chrome picks
  // different secrets for each user profile. This ensures that credentials are
  // logically tied to the Chrome user profile under which they were created.
  const std::string& secret_;

  DISALLOW_COPY_AND_ASSIGN(CredentialMetadata);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_CREDENTIAL_METADATA_H_
