// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/unknown_user_pref_accessor.h"

#include <iterator>
#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace sync_preferences {

UnknownUserPrefAccessor::UnknownUserPrefAccessor(
    PrefService* pref_service,
    user_prefs::PrefRegistrySyncable* pref_registry,
    PersistentPrefStore* user_prefs)
    : pref_service_(pref_service),
      pref_registry_(pref_registry),
      user_prefs_(user_prefs) {}

UnknownUserPrefAccessor::~UnknownUserPrefAccessor() {}

UnknownUserPrefAccessor::PreferenceState
UnknownUserPrefAccessor::GetPreferenceState(
    syncer::ModelType type,
    const std::string& pref_name) const {
  PreferenceState result;
  result.registration_state = GetRegistrationState(type, pref_name);
  switch (result.registration_state) {
    case RegistrationState::kUnknown:
    case RegistrationState::kUnknownWhitelisted:
      if (!user_prefs_->GetValue(pref_name, &result.persisted_value)) {
        result.persisted_value = nullptr;
      }
      break;
    case RegistrationState::kSyncable:
    case RegistrationState::kNotSyncable:
      result.persisted_value = pref_service_->GetUserPrefValue(pref_name);
      break;
  }
  return result;
}

void UnknownUserPrefAccessor::ClearPref(
    const std::string& pref_name,
    const PreferenceState& local_pref_state) {
  switch (local_pref_state.registration_state) {
    case RegistrationState::kUnknown:
      NOTREACHED() << "Sync attempted to update an unknown pref which is not "
                      "whitelisted: "
                   << pref_name;
      break;
    case RegistrationState::kUnknownWhitelisted:
      user_prefs_->RemoveValue(pref_name,
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
      break;
    case RegistrationState::kSyncable:
      pref_service_->ClearPref(pref_name);
      break;
    case RegistrationState::kNotSyncable:
      // As this can happen if different clients disagree about which
      // preferences should be synced, we only log a warning.
      DLOG(WARNING)
          << "Sync attempted to update a pref which is not registered as "
             "syncable. Ignoring the remote change for pref: "
          << pref_name;
      break;
  }
}

int UnknownUserPrefAccessor::GetNumberOfSyncingUnknownPrefs() const {
  return synced_unknown_prefs_.size();
}

namespace {

bool VerifyTypesBeforeSet(const std::string& pref_name,
                          const base::Value* local_value,
                          const base::Value& new_value) {
  if (local_value == nullptr || local_value->type() == new_value.type()) {
    return true;
  }
  UMA_HISTOGRAM_BOOLEAN("Sync.Preferences.RemotePrefTypeMismatch", true);
  DLOG(WARNING) << "Unexpected type mis-match for pref. "
                << "Synced value for " << pref_name << " is of type "
                << new_value.type() << " which doesn't match the locally "
                << "present pref type: " << local_value->type();
  return false;
}

}  // namespace

void UnknownUserPrefAccessor::SetPref(const std::string& pref_name,
                                      const PreferenceState& local_pref_state,
                                      const base::Value& value) {
  // On type mis-match, we trust the local preference DB and ignore the remote
  // change.
  switch (local_pref_state.registration_state) {
    case RegistrationState::kUnknown:
      NOTREACHED() << "Sync attempted to update a unknown pref which is not "
                      "whitelisted: "
                   << pref_name;
      break;
    case RegistrationState::kUnknownWhitelisted:
      if (VerifyTypesBeforeSet(pref_name, local_pref_state.persisted_value,
                               value)) {
        user_prefs_->SetValue(pref_name, value.CreateDeepCopy(),
                              WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
      }
      synced_unknown_prefs_.insert(pref_name);
      break;
    case RegistrationState::kSyncable:
      if (VerifyTypesBeforeSet(pref_name, local_pref_state.persisted_value,
                               value)) {
        pref_service_->Set(pref_name, value);
      }
      break;
    case RegistrationState::kNotSyncable:
      // As this can happen if different clients disagree about which
      // preferences should be synced, we only log a warning.
      DLOG(WARNING)
          << "Sync attempted to update a pref which is not registered as "
             "syncable. Ignoring the remote change for pref: "
          << pref_name;
      break;
  }
}

void UnknownUserPrefAccessor::EnforceRegisteredTypeInStore(
    const std::string& pref_name) {
  const base::Value* persisted_value = nullptr;
  if (user_prefs_->GetValue(pref_name, &persisted_value)) {
    // Get the registered type (typically from the default value).
    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name);
    DCHECK(pref);
    if (pref->GetType() != persisted_value->type()) {
      // We see conflicting type information and there's a chance the local
      // type-conflicting data came in via sync. Remove it.
      // TODO(tschumann): The value should get removed silently. Add a method
      // RemoveValueSilently() to WriteablePrefStore. Note, that as of today
      // that removal will only notify other pref stores but not sync -- that's
      // done on a higher level.
      user_prefs_->RemoveValue(pref_name,
                               WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
      UMA_HISTOGRAM_BOOLEAN("Sync.Preferences.ClearedLocalPrefOnTypeMismatch",
                            true);
    }
  }
  synced_unknown_prefs_.erase(pref_name);
}

UnknownUserPrefAccessor::RegistrationState
UnknownUserPrefAccessor::GetRegistrationState(
    syncer::ModelType type,
    const std::string& pref_name) const {
  uint32_t type_flag = 0;
  switch (type) {
    case syncer::PRIORITY_PREFERENCES:
      type_flag = user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF;
      break;
    case syncer::PREFERENCES:
      type_flag = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
      break;
    default:
      NOTREACHED() << "unexpected model type for preferences: " << type;
  }
  if (pref_registry_->defaults()->GetValue(pref_name, nullptr)) {
    uint32_t flags = pref_registry_->GetRegistrationFlags(pref_name);
    if (flags & type_flag) {
      return RegistrationState::kSyncable;
    }
    // Imagine the case where a preference has been synced as SYNCABLE_PREF
    // first and then got changed to SYNCABLE_PRIORITY_PREF:
    // In that situation, it could be argued for both, the preferences to be
    // considered unknown or not synced. However, as we plan to eventually also
    // sync unknown preferences, we cannot label them as unknown and treat them
    // as not synced instead. (The underlying problem is that priority
    // preferences are a concept only known to sync. The persistent stores don't
    // distinguish between those two).
    return RegistrationState::kNotSyncable;
  }
  if (pref_registry_->IsWhitelistedLateRegistrationPref(pref_name)) {
    return RegistrationState::kUnknownWhitelisted;
  }
  return RegistrationState::kUnknown;
}

}  // namespace sync_preferences
