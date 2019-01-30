// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_HASH_DATA_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_HASH_DATA_H_

#include <stdint.h>

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_piece_forward.h"

namespace password_manager {

struct PasswordHashData {
  PasswordHashData();
  PasswordHashData(const PasswordHashData& other);
  PasswordHashData(const std::string& username,
                   const base::string16& password,
                   bool force_update,
                   bool is_gaia_password = true);
  // Returns true iff |*this| represents the credential (|username|,
  // |password|), also with respect to whether it |is_gaia_password|.
  bool MatchesPassword(const std::string& username,
                       const base::string16& password,
                       bool is_gaia_password) const;

  std::string username;
  size_t length = 0;
  std::string salt;
  uint64_t hash = 0;
  bool force_update = false;
  bool is_gaia_password = true;
};

// SyncPasswordData is being deprecated. Please use PasswordHashData instead.
struct SyncPasswordData {
  SyncPasswordData() = default;
  SyncPasswordData(const SyncPasswordData& other) = default;
  SyncPasswordData(const base::string16& password, bool force_update);
  // Returns true iff |*this| represents |password|.
  bool MatchesPassword(const base::string16& password) const;

  size_t length = 0;
  std::string salt;
  uint64_t hash = 0;
  // Signal that we need to update password hash, salt, and length in profile
  // prefs.
  bool force_update = false;
};

// Calculates 37 bits hash for a password. The calculation is based on a slow
// hash function. The running time is ~10^{-4} seconds on Desktop.
uint64_t CalculatePasswordHash(const base::StringPiece16& text,
                               const std::string& salt);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_HASH_DATA_H_
