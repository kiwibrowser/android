// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_UNKNOWN_USER_PREF_ACCESSOR_H_
#define COMPONENTS_SYNC_PREFERENCES_UNKNOWN_USER_PREF_ACCESSOR_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/sync/base/model_type.h"

class PersistentPrefStore;
class PrefService;

namespace base {
class Value;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace sync_preferences {

// A class to access user prefs even before they were registered.
// Currently, accessing not registered (unknown) prefs is limited to a
// whitelist.
class UnknownUserPrefAccessor {
 public:
  enum class RegistrationState {
    kUnknown,  // Preference is not registered (on this Chrome instance).
    kUnknownWhitelisted,  // Preference is not registered but whitelisted to be
                          // synced without being registered.
    kSyncable,            // Preference is registered as being synced.
    kNotSyncable          // Preference is registered as not being synced.
  };

  struct PreferenceState {
    // The registration state of a preference.
    RegistrationState registration_state = RegistrationState::kUnknown;

    // The actually stored value. nullptr if no value is persisted and the pref
    // service serves a default value for this pref.
    // Ownership lies with the underlying pref-store.
    const base::Value* persisted_value = nullptr;
  };

  // |pref_service|, |pref_registry|, and |user_prefs| must not be null and must
  // outlive the lifetime of the created instance. The caller keeps ownership
  // over these objects.
  UnknownUserPrefAccessor(PrefService* pref_service,
                          user_prefs::PrefRegistrySyncable* pref_registry,
                          PersistentPrefStore* user_prefs);
  ~UnknownUserPrefAccessor();

  // Computes the state of a preference with name |pref_name| which gives
  // information about whether it's registered and the locally persisted value.
  PreferenceState GetPreferenceState(syncer::ModelType type,
                                     const std::string& pref_name) const;

  // Removes the value of the preference |pref_name| from the user prefstore.
  // Must not be called for preferences having RegistrationState::kUnknown.
  // When called for preferences registiered as not syncable
  // (RegistrationState::kNotSyncable), no changes to the storage are made.
  void ClearPref(const std::string& pref_name,
                 const PreferenceState& local_pref_state);

  // Changes the value of the preference |pref_name| on the user prefstore.
  // Must not be called for preferences having RegistrationState::kUnknown.
  // When called for preferences registiered as not syncable
  // (RegistrationState::kNotSyncable), no changes to the storage are made.
  void SetPref(const std::string& pref_name,
               const PreferenceState& local_pref_state,
               const base::Value& value);

  // Verifies that the type which preference |pref_name| was registered with
  // matches the type of any persisted value. On mismatch, the persisted value
  // gets removed.
  void EnforceRegisteredTypeInStore(const std::string& pref_name);

  // Returns the number of synced preferences which have not been registered (so
  // far).
  int GetNumberOfSyncingUnknownPrefs() const;

 private:
  RegistrationState GetRegistrationState(syncer::ModelType type,
                                         const std::string& pref_name) const;

  std::set<std::string> synced_unknown_prefs_;
  PrefService* const pref_service_;
  user_prefs::PrefRegistrySyncable* const pref_registry_;
  PersistentPrefStore* const user_prefs_;

  DISALLOW_COPY_AND_ASSIGN(UnknownUserPrefAccessor);
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_UNKNOWN_USER_PREF_ACCESSOR_H_
