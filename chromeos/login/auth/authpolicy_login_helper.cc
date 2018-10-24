// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/authpolicy_login_helper.h"

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/cryptohome/tpm_util.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart_client.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/symmetric_key.h"

namespace chromeos {


namespace {

constexpr char kAttrMode[] = "enterprise.mode";
constexpr char kDeviceModeEnterpriseAD[] = "enterprise_ad";
constexpr char kDCPrefix[] = "DC=";
constexpr char kOUPrefix[] = "OU=";

base::ScopedFD GetDataReadPipe(const std::string& data) {
  int pipe_fds[2];
  if (!base::CreateLocalNonBlockingPipe(pipe_fds)) {
    DLOG(ERROR) << "Failed to create pipe";
    return base::ScopedFD();
  }
  base::ScopedFD pipe_read_end(pipe_fds[0]);
  base::ScopedFD pipe_write_end(pipe_fds[1]);

  if (!base::WriteFileDescriptor(pipe_write_end.get(), data.c_str(),
                                 data.size())) {
    DLOG(ERROR) << "Failed to write to pipe";
    return base::ScopedFD();
  }
  return pipe_read_end;
}

bool ParseDomainAndOU(const std::string& distinguished_name,
                      authpolicy::JoinDomainRequest* request) {
  std::string machine_domain;
  std::vector<std::string> split_dn =
      base::SplitString(distinguished_name, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& str : split_dn) {
    if (base::StartsWith(str, kOUPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      *request->add_machine_ou() = str.substr(strlen(kOUPrefix));
    } else if (base::StartsWith(str, kDCPrefix,
                                base::CompareCase::INSENSITIVE_ASCII)) {
      if (!machine_domain.empty())
        machine_domain.append(".");
      machine_domain.append(str.substr(strlen(kDCPrefix)));
    } else {
      return false;
    }
  }
  if (!machine_domain.empty())
    request->set_machine_domain(machine_domain);
  return true;
}

std::string DoDecrypt(const std::string& encrypted_data,
                      const std::string& password) {
  constexpr char error_msg[] = "Failed to decrypt data";
  const size_t kSaltSize = 32;
  const size_t kSignatureSize = 32;
  if (encrypted_data.size() <= kSaltSize + kSignatureSize) {
    LOG(ERROR) << error_msg;
    return std::string();
  }

  const std::string salt = encrypted_data.substr(0, kSaltSize);
  const std::string signature =
      encrypted_data.substr(kSaltSize, kSignatureSize);
  const std::string ciphertext =
      encrypted_data.substr(kSaltSize + kSignatureSize);

  // Derive AES key, AES IV and HMAC key from password.
  const size_t kAesKeySize = 32;
  const size_t kAesIvSize = 16;
  const size_t kHmacKeySize = 32;
  const size_t kKeySize = kAesKeySize + kAesIvSize + kHmacKeySize;
  std::unique_ptr<crypto::SymmetricKey> key =
      crypto::SymmetricKey::DeriveKeyFromPassword(
          crypto::SymmetricKey::HMAC_SHA1, password, salt, 10000, kKeySize * 8);
  if (!key) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  DCHECK(kAesKeySize + kAesIvSize + kHmacKeySize == key->key().size());
  const char* key_data_chars = key->key().data();
  std::string aes_key(key_data_chars, kAesKeySize);
  std::string aes_iv(key_data_chars + kAesKeySize, kAesIvSize);
  std::string hmac_key(key_data_chars + kAesKeySize + kAesIvSize, kHmacKeySize);

  // Check signature.
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (kSignatureSize != hmac.DigestLength()) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  uint8_t recomputed_signature[kSignatureSize];
  if (!hmac.Init(hmac_key) ||
      !hmac.Sign(ciphertext, recomputed_signature, kSignatureSize)) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  std::string recomputed_signature_str(
      reinterpret_cast<char*>(recomputed_signature), kSignatureSize);
  if (signature != recomputed_signature_str) {
    LOG(ERROR) << error_msg;
    return std::string();
  }

  // Decrypt.
  std::unique_ptr<crypto::SymmetricKey> aes_key_obj(
      crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, aes_key));
  crypto::Encryptor encryptor;
  if (!encryptor.Init(aes_key_obj.get(), crypto::Encryptor::CBC, aes_iv)) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  std::string decrypted_data;
  if (!encryptor.Decrypt(ciphertext, &decrypted_data)) {
    LOG(ERROR) << error_msg;
    return std::string();
  }
  return decrypted_data;
}

}  // namespace

AuthPolicyLoginHelper::AuthPolicyLoginHelper() : weak_factory_(this) {}

// static
void AuthPolicyLoginHelper::TryAuthenticateUser(const std::string& username,
                                                const std::string& object_guid,
                                                const std::string& password) {
  authpolicy::AuthenticateUserRequest request;
  request.set_user_principal_name(username);
  request.set_account_id(object_guid);
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->AuthenticateUser(
      request, GetDataReadPipe(password).get(), base::DoNothing());
}

// static
void AuthPolicyLoginHelper::Restart() {
  chromeos::DBusThreadManager::Get()
      ->GetUpstartClient()
      ->RestartAuthPolicyService();
}

// static
void AuthPolicyLoginHelper::DecryptConfiguration(const std::string& blob,
                                                 const std::string& password,
                                                 OnDecryptedCallback callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&DoDecrypt, blob, password), std::move(callback));
}

// static
bool AuthPolicyLoginHelper::IsAdLocked() {
  std::string mode;
  return chromeos::tpm_util::InstallAttributesGet(kAttrMode, &mode) &&
         mode == kDeviceModeEnterpriseAD;
}

// static
bool AuthPolicyLoginHelper::LockDeviceActiveDirectoryForTesting(
    const std::string& realm) {
  return tpm_util::InstallAttributesSet("enterprise.owned", "true") &&
         tpm_util::InstallAttributesSet(kAttrMode, kDeviceModeEnterpriseAD) &&
         tpm_util::InstallAttributesSet("enterprise.realm", realm) &&
         tpm_util::InstallAttributesFinalize();
}

void AuthPolicyLoginHelper::JoinAdDomain(const std::string& machine_name,
                                         const std::string& distinguished_name,
                                         int encryption_types,
                                         const std::string& username,
                                         const std::string& password,
                                         JoinCallback callback) {
  DCHECK(!IsAdLocked());
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  authpolicy::JoinDomainRequest request;
  if (!ParseDomainAndOU(distinguished_name, &request)) {
    DLOG(ERROR) << "Failed to parse computer distinguished name";
    std::move(callback).Run(authpolicy::ERROR_INVALID_OU, std::string());
    return;
  }
  if (!machine_name.empty())
    request.set_machine_name(machine_name);
  DCHECK(authpolicy::KerberosEncryptionTypes_IsValid(encryption_types));
  request.set_kerberos_encryption_types(
      static_cast<authpolicy::KerberosEncryptionTypes>(encryption_types));
  if (!username.empty())
    request.set_user_principal_name(username);
  DCHECK(!dm_token_.empty());
  request.set_dm_token(dm_token_);

  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->JoinAdDomain(
      request, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyLoginHelper::OnJoinCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthPolicyLoginHelper::AuthenticateUser(const std::string& username,
                                             const std::string& object_guid,
                                             const std::string& password,
                                             AuthCallback callback) {
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  authpolicy::AuthenticateUserRequest request;
  request.set_user_principal_name(username);
  request.set_account_id(object_guid);
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->AuthenticateUser(
      request, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyLoginHelper::OnAuthCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AuthPolicyLoginHelper::CancelRequestsAndRestart() {
  weak_factory_.InvalidateWeakPtrs();
  dm_token_.clear();
  AuthPolicyLoginHelper::Restart();
}

void AuthPolicyLoginHelper::OnJoinCallback(JoinCallback callback,
                                           authpolicy::ErrorType error,
                                           const std::string& machine_domain) {
  DCHECK(!IsAdLocked());
  if (error != authpolicy::ERROR_NONE) {
    std::move(callback).Run(error, machine_domain);
    return;
  }
  chromeos::DBusThreadManager::Get()
      ->GetAuthPolicyClient()
      ->RefreshDevicePolicy(base::BindOnce(
          &AuthPolicyLoginHelper::OnFirstPolicyRefreshCallback,
          weak_factory_.GetWeakPtr(), std::move(callback), machine_domain));
}

void AuthPolicyLoginHelper::OnFirstPolicyRefreshCallback(
    JoinCallback callback,
    const std::string& machine_domain,
    authpolicy::ErrorType error) {
  DCHECK(!IsAdLocked());
  // First policy refresh happens before device is locked. So policy store
  // should not succeed. The error means that authpolicyd cached device policy
  // and stores it in the next call to RefreshDevicePolicy in STEP_STORE_POLICY.
  DCHECK(error != authpolicy::ERROR_NONE);
  if (error == authpolicy::ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT)
    error = authpolicy::ERROR_NONE;
  std::move(callback).Run(error, machine_domain);
}

void AuthPolicyLoginHelper::OnAuthCallback(
    AuthCallback callback,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  std::move(callback).Run(error, account_info);
}

AuthPolicyLoginHelper::~AuthPolicyLoginHelper() = default;

}  // namespace chromeos
